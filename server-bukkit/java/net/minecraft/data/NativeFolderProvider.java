// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CompletableFuture;

/** Copies data pack folders owned by the native pack service into the generated output. */
public class NativeFolderProvider implements DataProvider {
    private final PackOutput output;
    private final String name;
    private final String[] folders;

    public NativeFolderProvider(PackOutput output, String name, String... folders) {
        this.output = output;
        this.name = name;
        this.folders = folders;
    }

    @Override
    public CompletableFuture<?> run(CachedOutput cache) {
        List<CompletableFuture<?>> writes = new ArrayList<>();
        for (String folder : folders) {
            writes.add(NativeData.copyFolder(cache, output, folder));
        }
        return CompletableFuture.allOf(writes.toArray(CompletableFuture[]::new));
    }

    @Override
    public String getName() {
        return name;
    }
}
