// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data;

import java.nio.file.Path;
import net.minecraft.core.Registry;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;

public class PackOutput {
    private final Path outputFolder;

    public PackOutput(Path outputFolder) {
        this.outputFolder = outputFolder;
    }

    public Path getOutputFolder() {
        return outputFolder;
    }

    public Path getOutputFolder(Target target) {
        return getOutputFolder().resolve(target.directory);
    }

    public PathProvider createPathProvider(Target target, String kind) {
        return new PathProvider(this, target, kind);
    }

    public PathProvider createRegistryElementsPathProvider(ResourceKey<? extends Registry<?>> registryKey) {
        return createPathProvider(Target.DATA_PACK, registryDirectory(registryKey));
    }

    public PathProvider createRegistryTagsPathProvider(ResourceKey<? extends Registry<?>> registryKey) {
        return createPathProvider(Target.DATA_PACK, "tags/" + registryDirectory(registryKey));
    }

    public PathProvider createRegistryComponentPathProvider(ResourceKey<? extends Registry<?>> registryKey) {
        return createPathProvider(Target.DATA_PACK, "component/" + registryDirectory(registryKey));
    }

    private static String registryDirectory(ResourceKey<? extends Registry<?>> registryKey) {
        Identifier id = registryKey.identifier();
        return id.getNamespace().equals("minecraft") ? id.getPath() : id.getNamespace() + "/" + id.getPath();
    }

    public static enum Target {
        DATA_PACK("data"), RESOURCE_PACK("assets"), REPORTS("reports");

        final String directory;

        Target(String directory) {
            this.directory = directory;
        }
    }

    public static class PathProvider {
        private final Path root;
        private final String kind;

        private PathProvider(PackOutput output, Target target, String kind) {
            this.root = output.getOutputFolder(target);
            this.kind = kind;
        }

        public Path file(Identifier element, String extension) {
            return root.resolve(element.getNamespace()).resolve(kind).resolve(element.getPath() + "." + extension);
        }

        public Path json(Identifier element) {
            return file(element, "json");
        }

        public Path json(ResourceKey<?> element) {
            return json(element.identifier());
        }
    }
}
