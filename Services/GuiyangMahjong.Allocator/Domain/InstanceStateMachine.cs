namespace GuiyangMahjong.Allocator.Domain;

internal static class InstanceStateMachine
{
    private static readonly IReadOnlyDictionary<GameServerInstanceState, GameServerInstanceState[]> Allowed =
        new Dictionary<GameServerInstanceState, GameServerInstanceState[]>
        {
            [GameServerInstanceState.Starting] = [GameServerInstanceState.Ready, GameServerInstanceState.Failed],
            [GameServerInstanceState.Ready] = [GameServerInstanceState.Allocated, GameServerInstanceState.Failed],
            [GameServerInstanceState.Allocated] = [GameServerInstanceState.Draining, GameServerInstanceState.Failed],
            [GameServerInstanceState.Draining] = [GameServerInstanceState.Stopped, GameServerInstanceState.Failed],
            [GameServerInstanceState.Stopped] = [],
            [GameServerInstanceState.Failed] = [GameServerInstanceState.Stopped]
        };

    public static void Transition(GameServerInstance instance, GameServerInstanceState next)
    {
        if (instance.State == next) return;
        if (!Allowed[instance.State].Contains(next))
        {
            throw new InvalidOperationException($"实例状态不允许从 {instance.State} 转换到 {next}");
        }
        instance.State = next;
    }
}
