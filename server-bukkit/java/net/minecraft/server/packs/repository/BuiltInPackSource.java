// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.repository;
import java.io.IOException;
import java.nio.file.Files;
import java.util.HashMap;
import java.util.Map;
import java.nio.file.Path;
import java.util.function.BiConsumer;
import java.util.function.Consumer;
import java.util.function.Function;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
import net.minecraft.server.packs.PackType;
import net.minecraft.server.packs.VanillaPackResources;
import net.minecraft.world.level.validation.DirectoryValidator;
public abstract class BuiltInPackSource implements RepositorySource {
    public static final String VANILLA_ID = "vanilla";
    public static final String TESTS_ID = "tests";
    public static final KnownPack CORE_PACK_INFO = KnownPack.vanilla("core");
    private final PackType packType;
    private final VanillaPackResources vanillaPack;
    private final Identifier packDir;
    private final DirectoryValidator validator;

    public BuiltInPackSource(PackType packType, VanillaPackResources vanillaPack, Identifier packDir, DirectoryValidator validator) {
        this.packType = packType;
        this.vanillaPack = vanillaPack;
        this.packDir = packDir;
        this.validator = validator;
        
    }
    public void loadPacks(Consumer<Pack> result) {
        Pack vanilla = createVanillaPack(vanillaPack.asResourcesSupplier());
        if (vanilla != null) {
            result.accept(vanilla);
        }
        Map<String, Function<String, Pack>> discovered = new HashMap<>();
        populatePackList(discovered::put);
        discovered.forEach((id, factory) -> {
            Pack pack = factory.apply(id);
            if (pack != null) {
                result.accept(pack);
            }
        });
    }
    protected abstract Pack createVanillaPack(Pack.ResourcesSupplier resources);
    protected abstract Component getPackTitle(String id);
    public VanillaPackResources getVanillaPack() {
        return vanillaPack;
    }
    protected void populatePackList(BiConsumer<String, Function<String, Pack>> discoveredPacks) {
        vanillaPack.listRawPaths(packType, packDir, path -> discoverPacksInPath(path, discoveredPacks));
    }
    protected void discoverPacksInPath(Path targetDir, BiConsumer<String, Function<String, Pack>> discoveredPacks) {
        if (targetDir == null || !Files.isDirectory(targetDir)) {
            return;
        }
        try {
            FolderRepositorySource.discoverPacks(targetDir, validator, (path, resources) -> {
                String name = path.getFileName().toString();
                String id = name.endsWith(".zip") ? name.substring(0, name.length() - 4) : name;
                discoveredPacks.accept(id, key -> createBuiltinPack(key, resources, getPackTitle(key)));
            });
        } catch (IOException unreadable) {
            return;
        }
    }
    protected abstract Pack createBuiltinPack(String id, Pack.ResourcesSupplier resources, Component name);
}
