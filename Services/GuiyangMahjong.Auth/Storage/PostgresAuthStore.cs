using System.Security.Cryptography;
using GuiyangMahjong.Auth.Domain;
using Npgsql;

namespace GuiyangMahjong.Auth.Storage;

public sealed class PostgresAuthStore(NpgsqlDataSource postgres) : IAuthStore, IAsyncDisposable
{
    public async Task InitializeAsync(CancellationToken cancellationToken)
    {
        var path = Path.Combine(AppContext.BaseDirectory, "Storage", "schema.sql");
        await using var command = postgres.CreateCommand(await File.ReadAllTextAsync(path, cancellationToken));
        await command.ExecuteNonQueryAsync(cancellationToken);
    }

    public async Task<bool> CheckHealthAsync(CancellationToken cancellationToken)
    {
        try
        {
            await using var command = postgres.CreateCommand("SELECT 1");
            _ = await command.ExecuteScalarAsync(cancellationToken);
            return true;
        }
        catch (NpgsqlException)
        {
            return false;
        }
    }

    public async Task<AuthIdentity> GetOrCreateGuestAsync(
        string installationHash,
        AuthIdentity proposedIdentity,
        CancellationToken cancellationToken)
    {
        await using var command = postgres.CreateCommand(
            """
            INSERT INTO auth_identities(
                installation_hash, player_id, display_name, provider, created_at_utc, updated_at_utc)
            VALUES ($1, $2, $3, $4, $5, $6)
            ON CONFLICT (installation_hash) DO UPDATE
                SET updated_at_utc = auth_identities.updated_at_utc
            RETURNING player_id, display_name, provider, created_at_utc, updated_at_utc
            """);
        command.Parameters.AddWithValue(installationHash);
        command.Parameters.AddWithValue(proposedIdentity.PlayerId);
        command.Parameters.AddWithValue(proposedIdentity.DisplayName);
        command.Parameters.AddWithValue(proposedIdentity.Provider);
        command.Parameters.AddWithValue(proposedIdentity.CreatedAtUtc);
        command.Parameters.AddWithValue(proposedIdentity.UpdatedAtUtc);
        await using var reader = await command.ExecuteReaderAsync(cancellationToken);
        if (!await reader.ReadAsync(cancellationToken))
            throw new InvalidDataException("Auth identity upsert returned no row.");
        return ReadIdentity(reader);
    }

    public async Task CreateRefreshSessionAsync(RefreshSession session, CancellationToken cancellationToken)
    {
        await using var command = postgres.CreateCommand(
            """
            INSERT INTO auth_refresh_sessions(
                session_id, player_id, token_hash, expires_at_utc, created_at_utc, revoked_at_utc)
            VALUES ($1, $2, $3, $4, $5, $6)
            """);
        AddSessionParameters(command, session);
        await command.ExecuteNonQueryAsync(cancellationToken);
    }

    public async Task<RefreshRotationResult> RotateRefreshSessionAsync(
        string currentSessionId,
        byte[] currentTokenHash,
        RefreshSession replacement,
        DateTimeOffset now,
        CancellationToken cancellationToken)
    {
        await using var connection = await postgres.OpenConnectionAsync(cancellationToken);
        await using var transaction = await connection.BeginTransactionAsync(cancellationToken);
        await using var select = new NpgsqlCommand(
            """
            SELECT session.player_id, session.token_hash, session.expires_at_utc, session.revoked_at_utc,
                   identity.display_name, identity.provider, identity.created_at_utc, identity.updated_at_utc
            FROM auth_refresh_sessions AS session
            JOIN auth_identities AS identity ON identity.player_id = session.player_id
            WHERE session.session_id = $1
            FOR UPDATE OF session
            """, connection, transaction);
        select.Parameters.AddWithValue(currentSessionId);
        await using var reader = await select.ExecuteReaderAsync(cancellationToken);
        if (!await reader.ReadAsync(cancellationToken))
        {
            await reader.DisposeAsync();
            await transaction.RollbackAsync(cancellationToken);
            return new RefreshRotationResult(RefreshRotationStatus.NotFound, null);
        }
        var playerId = reader.GetString(0);
        var storedHash = reader.GetFieldValue<byte[]>(1);
        var expiresAt = reader.GetFieldValue<DateTimeOffset>(2);
        DateTimeOffset? revokedAt = reader.IsDBNull(3) ? null : reader.GetFieldValue<DateTimeOffset>(3);
        var identity = new AuthIdentity(
            playerId,
            reader.GetString(4),
            reader.GetString(5),
            reader.GetFieldValue<DateTimeOffset>(6),
            reader.GetFieldValue<DateTimeOffset>(7));
        await reader.DisposeAsync();

        var status = !FixedTimeEquals(storedHash, currentTokenHash)
            ? RefreshRotationStatus.Invalid
            : revokedAt is not null
                ? RefreshRotationStatus.Revoked
                : expiresAt <= now
                    ? RefreshRotationStatus.Expired
                    : RefreshRotationStatus.Rotated;
        if (status != RefreshRotationStatus.Rotated)
        {
            await transaction.RollbackAsync(cancellationToken);
            return new RefreshRotationResult(status, null);
        }

        await using var revoke = new NpgsqlCommand(
            "UPDATE auth_refresh_sessions SET revoked_at_utc=$1 WHERE session_id=$2",
            connection, transaction);
        revoke.Parameters.AddWithValue(now);
        revoke.Parameters.AddWithValue(currentSessionId);
        await revoke.ExecuteNonQueryAsync(cancellationToken);
        await using var insert = new NpgsqlCommand(
            """
            INSERT INTO auth_refresh_sessions(
                session_id, player_id, token_hash, expires_at_utc, created_at_utc, revoked_at_utc)
            VALUES ($1, $2, $3, $4, $5, $6)
            """, connection, transaction);
        AddSessionParameters(insert, replacement with { PlayerId = playerId });
        await insert.ExecuteNonQueryAsync(cancellationToken);
        await transaction.CommitAsync(cancellationToken);
        return new RefreshRotationResult(RefreshRotationStatus.Rotated, identity);
    }

    public async Task<bool> RevokeRefreshSessionAsync(
        string sessionId,
        byte[] tokenHash,
        DateTimeOffset now,
        CancellationToken cancellationToken)
    {
        await using var connection = await postgres.OpenConnectionAsync(cancellationToken);
        await using var transaction = await connection.BeginTransactionAsync(cancellationToken);
        await using var select = new NpgsqlCommand(
            "SELECT token_hash, revoked_at_utc FROM auth_refresh_sessions WHERE session_id=$1 FOR UPDATE",
            connection, transaction);
        select.Parameters.AddWithValue(sessionId);
        await using var reader = await select.ExecuteReaderAsync(cancellationToken);
        if (!await reader.ReadAsync(cancellationToken))
        {
            await reader.DisposeAsync();
            await transaction.RollbackAsync(cancellationToken);
            return false;
        }
        var matches = reader.IsDBNull(1)
                      && FixedTimeEquals(reader.GetFieldValue<byte[]>(0), tokenHash);
        await reader.DisposeAsync();
        if (!matches)
        {
            await transaction.RollbackAsync(cancellationToken);
            return false;
        }
        await using var revoke = new NpgsqlCommand(
            "UPDATE auth_refresh_sessions SET revoked_at_utc=$1 WHERE session_id=$2",
            connection, transaction);
        revoke.Parameters.AddWithValue(now);
        revoke.Parameters.AddWithValue(sessionId);
        await revoke.ExecuteNonQueryAsync(cancellationToken);
        await transaction.CommitAsync(cancellationToken);
        return true;
    }

    private static AuthIdentity ReadIdentity(NpgsqlDataReader reader) => new(
        reader.GetString(0),
        reader.GetString(1),
        reader.GetString(2),
        reader.GetFieldValue<DateTimeOffset>(3),
        reader.GetFieldValue<DateTimeOffset>(4));

    private static void AddSessionParameters(NpgsqlCommand command, RefreshSession session)
    {
        command.Parameters.AddWithValue(session.SessionId);
        command.Parameters.AddWithValue(session.PlayerId);
        command.Parameters.AddWithValue(session.TokenHash);
        command.Parameters.AddWithValue(session.ExpiresAtUtc);
        command.Parameters.AddWithValue(session.CreatedAtUtc);
        command.Parameters.AddWithValue((object?)session.RevokedAtUtc ?? DBNull.Value);
    }

    private static bool FixedTimeEquals(byte[] left, byte[] right) =>
        left.Length == right.Length && CryptographicOperations.FixedTimeEquals(left, right);

    public ValueTask DisposeAsync() => postgres.DisposeAsync();
}
