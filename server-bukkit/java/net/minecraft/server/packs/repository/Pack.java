// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.repository;
import net.minecraft.SharedConstants;
import net.minecraft.server.packs.*;
import net.minecraft.server.packs.metadata.pack.*;
import net.minecraft.server.packs.FeatureFlagsMetadataSection;

import java.util.List;
import java.util.function.Function;
import java.util.stream.Stream;
import net.minecraft.network.chat.Component;
import net.minecraft.server.packs.PackLocationInfo;
import net.minecraft.server.packs.PackMetadataResources;
import net.minecraft.server.packs.PackResources;
import net.minecraft.server.packs.PackSelectionConfig;
import net.minecraft.server.packs.PackType;
import net.minecraft.world.flag.FeatureFlagSet;

public class Pack {
    private final PackLocationInfo location;
    private final ResourcesSupplier resources;
    private final Metadata metadata;
    private final PackSelectionConfig selectionConfig;

    public Pack(PackLocationInfo location, ResourcesSupplier resources, Metadata metadata, PackSelectionConfig selectionConfig) {
        this.location = location;
        this.resources = resources;
        this.metadata = metadata;
        this.selectionConfig = selectionConfig;
    }

    public static Pack readMetaAndCreate(PackLocationInfo location, ResourcesSupplier resources, PackType packType, PackSelectionConfig selectionConfig) {
        Metadata meta = readPackMetadata(location, resources, SharedConstants.getCurrentVersion().packVersion(packType), packType);
        return meta == null ? null : new Pack(location, resources, meta, selectionConfig);
    }

    public static Metadata readPackMetadata(PackLocationInfo location, ResourcesSupplier resources, PackFormat currentPackVersion, PackType type) {
        try (PackMetadataResources pack = resources.openMetadata(location)) {
            PackMetadataSection section;
            try {
                section = pack.getMetadataSection(PackMetadataSection.forPackType(type));
            } catch (com.google.gson.JsonParseException malformed) {
                section = pack.getMetadataSection(PackMetadataSection.FALLBACK_TYPE);
            }
            if (section == null) {
                return null;
            }
            FeatureFlagsMetadataSection flags = pack.getMetadataSection(FeatureFlagsMetadataSection.TYPE);
            OverlayMetadataSection overlays = pack.getMetadataSection(OverlayMetadataSection.forPackType(type));
            return new Metadata(section.description(), PackCompatibility.forVersion(section.supportedFormats(), currentPackVersion),
                flags != null ? flags.flags() : FeatureFlagSet.of(), overlays != null ? overlays.overlaysForVersion(currentPackVersion) : List.of());
        } catch (Exception unreadable) {
            return null;
        }
    }

    public PackLocationInfo location() {
        return location;
    }

    public Component getTitle() {
        return location.title();
    }

    public Component getDescription() {
        return metadata.description();
    }

    public Component getChatLink(boolean enabled) {
        return location.createChatLink(enabled, metadata.description());
    }

    public PackCompatibility getCompatibility() {
        return metadata.compatibility();
    }

    public FeatureFlagSet getRequestedFeatures() {
        return metadata.requestedFeatures();
    }

    public PackMetadataResources openMetadata() {
        return resources.openMetadata(location);
    }

    public Stream<PackResources> open() {
        return resources.openResources(location, metadata);
    }

    public String getId() {
        return location.id();
    }

    public PackSelectionConfig selectionConfig() {
        return selectionConfig;
    }

    public boolean isRequired() {
        return selectionConfig.required();
    }

    public boolean isFixedPosition() {
        return selectionConfig.fixedPosition();
    }

    public Position getDefaultPosition() {
        return selectionConfig.defaultPosition();
    }

    public PackSource getPackSource() {
        return location.source();
    }

    public boolean equals(Object other) {
        return other instanceof Pack pack && location.id().equals(pack.location.id());
    }

    public int hashCode() {
        return location.id().hashCode();
    }

    public static record Metadata(Component description, PackCompatibility compatibility, FeatureFlagSet requestedFeatures, List<String> overlays) {}

    public static enum Position {
        TOP, BOTTOM;

        public <T> int insert(List<T> list, T value, Function<T, PackSelectionConfig> converter, boolean reverse) {
            Position position = reverse ? opposite() : this;
            if (position == BOTTOM) {
                int index = 0;
                while (index < list.size() && converter.apply(list.get(index)).fixedPosition()) {
                    index++;
                }
                list.add(index, value);
                return index;
            }
            int index = list.size() - 1;
            while (index >= 0 && converter.apply(list.get(index)).fixedPosition()) {
                index--;
            }
            list.add(index + 1, value);
            return index + 1;
        }

        public Position opposite() {
            return this == TOP ? BOTTOM : TOP;
        }
    }

    public interface ResourcesSupplier {
        PackMetadataResources openMetadata(PackLocationInfo location);

        Stream<PackResources> openResources(PackLocationInfo location, Metadata metadata);
    }
}
