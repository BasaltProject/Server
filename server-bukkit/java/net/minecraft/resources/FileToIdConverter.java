// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.resources;
import java.util.List;
import java.util.Map;
import net.minecraft.core.Registry;
import net.minecraft.server.packs.resources.Resource;
import net.minecraft.server.packs.resources.ResourceManager;
public record FileToIdConverter(String prefix, String extension) {
    public static FileToIdConverter json(String prefix) {
        return new FileToIdConverter(prefix, ".json");
    }
    public static FileToIdConverter registry(ResourceKey<? extends Registry<?>> registry) {
        return json(net.minecraft.core.registries.Registries.elementsDirPath(registry));
    }
    public Identifier idToFile(Identifier id) {
        return id.withPath(prefix + "/" + id.getPath() + extension);
    }
    public Identifier fileToId(Identifier file) {
        String path = file.getPath();
        return file.withPath(path.substring(prefix.length() + 1, path.length() - extension.length()));
    }
    public boolean extensionMatches(Identifier id) {
        return id.getPath().endsWith(extension);
    }
    public boolean prefixMatches(Identifier id) {
        return id.getPath().startsWith(prefix);
    }
    public boolean matches(Identifier id) {
        return prefixMatches(id) && extensionMatches(id);
    }
    public Map<Identifier, Resource> listMatchingResources(ResourceManager manager) {
        return manager.listResources(prefix, this::extensionMatches);
    }
    public Map<Identifier, List<Resource>> listMatchingResourceStacks(ResourceManager manager) {
        return manager.listResourceStacks(prefix, this::extensionMatches);
    }
}
