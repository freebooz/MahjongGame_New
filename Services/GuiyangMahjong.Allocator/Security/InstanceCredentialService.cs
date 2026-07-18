using System.Security.Cryptography;

namespace GuiyangMahjong.Allocator.Security;

public sealed record GeneratedCredential(string Plaintext, byte[] Hash);

public sealed class InstanceCredentialService
{
    public GeneratedCredential Generate()
    {
        var plaintext = Convert.ToBase64String(RandomNumberGenerator.GetBytes(32));
        return new GeneratedCredential(plaintext, SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(plaintext)));
    }

    public bool Verify(string plaintext, byte[] expectedHash)
    {
        if (string.IsNullOrWhiteSpace(plaintext) || plaintext.Length > 256) return false;
        var actual = SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(plaintext));
        return CryptographicOperations.FixedTimeEquals(actual, expectedHash);
    }
}

