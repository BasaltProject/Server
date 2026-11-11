// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data;

import java.io.IOException;
import java.nio.file.Path;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import net.minecraft.WorldVersion;

public abstract class DataGenerator {
    protected final PackOutput vanillaPackOutput;
    protected final Set<String> allProviderIds = new LinkedHashSet<>();
    protected final Map<String, DataProvider> providersToRun = new LinkedHashMap<>();

    public DataGenerator(Path output) {
        this.vanillaPackOutput = new PackOutput(output);
    }

    public abstract void run() throws IOException;

    public PackGenerator getVanillaPack(boolean toRun) {
        return new PackGenerator(toRun, "vanilla", vanillaPackOutput);
    }

    public PackGenerator getBuiltinDatapack(boolean toRun, String packId) {
        Path packFolder = vanillaPackOutput.getOutputFolder(PackOutput.Target.DATA_PACK).resolve("minecraft").resolve("datapacks").resolve(packId);
        return new PackGenerator(toRun, packId, new PackOutput(packFolder));
    }

    void runProviders(CachedOutput output) {
        for (DataProvider provider : providersToRun.values()) {
            provider.run(output).join();
        }
    }

    public static class Cached extends DataGenerator {
        public Cached(Path output, WorldVersion version, boolean alwaysGenerate) {
            super(output);
        }

        @Override
        public void run() throws IOException {
            runProviders(CachedOutput.NO_CACHE);
        }
    }

    public static class Uncached extends DataGenerator {
        public Uncached(Path output) {
            super(output);
        }

        @Override
        public void run() throws IOException {
            runProviders(CachedOutput.NO_CACHE);
        }
    }

    public class PackGenerator {
        private final boolean toRun;
        private final String providerPrefix;
        private final PackOutput output;

        private PackGenerator(boolean toRun, String providerPrefix, PackOutput output) {
            this.toRun = toRun;
            this.providerPrefix = providerPrefix;
            this.output = output;
        }

        public <T extends DataProvider> T addProvider(DataProvider.Factory<T> factory) {
            T provider = factory.create(output);
            String id = providerPrefix + "/" + provider.getName();
            if (!allProviderIds.add(id)) {
                throw new IllegalStateException("Duplicate provider: " + id);
            }
            if (toRun) {
                providersToRun.put(id, provider);
            }
            return provider;
        }
    }
}
