// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.locks.ReentrantLock;
import java.util.HashMap;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.Interval;
public class DensityFunctionCompiler {
    private final DensityFunction.CompileContext context;
    private final Map<DensityFunction, DensitySampler> samplers = new ConcurrentHashMap<>();
    private final Map<DensityFunction, PreparedCache> prepared = new HashMap<>();
    private final ReentrantLock lock = new ReentrantLock();
    private final DfRewriteRule optimizer;
    private int nextCacheId;
    public DensityFunctionCompiler(DensityFunction.CompileContext context) {
        this.context = context;
        DfRewriteRule[] self = new DfRewriteRule[1];
        self[0] = DfRewriteRule.sequence(function -> {
            DensityFunction inlined = DfRewriteRule.INLINE_REFERENCE.rewrite(function);
            return inlined instanceof net.minecraft.world.level.levelgen.densityfunction.op.CacheFunction cache ? reuseOrPrepare(cache) : inlined.rewriteChildren(self[0]);
        }, DfRewriteRule.SLICE_UNIFORM_AXES);
        this.optimizer = self[0];
    }
    private DensitySampler optimizeAndCompile(DensityFunction function) {
        lock.lock();
        try {
            return optimizer.rewrite(function).compileSampler(context);
        } finally {
            lock.unlock();
        }
    }
    private DensityFunction reuseOrPrepare(net.minecraft.world.level.levelgen.densityfunction.op.CacheFunction cache) {
        PreparedCache found = prepared.get(cache.input());
        if (found == null) {
            DensityFunction inner = optimizer.rewrite(cache.input());
            int id = nextCacheId++;
            found = new PreparedCache(id, inner.range(), inner.domainAxes(), new CachingDensitySampler(id, inner.compileSampler(context)));
            prepared.put(cache.input(), found);
        }
        return found;
    }
    public DensitySampler getSampler(DensityFunction function) {
        return samplers.computeIfAbsent(function, this::optimizeAndCompile);

    }
    static record PreparedCache(int id, Interval range, int domainAxes, DensitySampler cachingSampler) implements DensityFunction {
        public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        return cachingSampler;
        }
        public DensityFunction rewriteChildren(DfRewriteRule rule) {
        return this;
        }
        public MapCodec<PreparedCache> codec() {
        throw new UnsupportedOperationException("A prepared cache is never encoded");

        }
    }
}
