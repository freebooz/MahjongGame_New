using GuiyangMahjong.Allocator.Domain;
using GuiyangMahjong.Allocator.Options;
using Microsoft.Extensions.Options;

namespace GuiyangMahjong.Allocator.Services;

public sealed class PortLeasePool
{
    private readonly SortedSet<int> available;
    private readonly HashSet<int> leased = [];
    private readonly Lock gate = new();

    public PortLeasePool(IOptions<AllocatorOptions> options)
    {
        available = new SortedSet<int>(Enumerable.Range(
            options.Value.PortStart,
            options.Value.PortEnd - options.Value.PortStart + 1));
    }

    public int Acquire()
    {
        lock (gate)
        {
            if (available.Count == 0) throw new AllocatorOperationException("没有可用的 GameServer 端口", 503);
            var port = available.Min;
            available.Remove(port);
            leased.Add(port);
            return port;
        }
    }

    public bool Release(int port)
    {
        lock (gate)
        {
            if (!leased.Remove(port)) return false;
            available.Add(port);
            return true;
        }
    }

    public int AvailableCount
    {
        get { lock (gate) return available.Count; }
    }
}
