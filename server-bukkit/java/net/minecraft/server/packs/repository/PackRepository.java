// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.repository;

import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.stream.Collectors;
import java.util.stream.Stream;
import net.minecraft.ChatFormatting;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentUtils;
import net.minecraft.server.packs.PackLocationInfo;
import net.minecraft.server.packs.PackMetadataResources;
import net.minecraft.server.packs.PackResources;
import net.minecraft.server.packs.PackSelectionConfig;
import net.minecraft.world.flag.FeatureFlagSet;
import org.bukkit.Bukkit;
import org.bukkit.FeatureFlag;
import io.papermc.paper.datapack.Datapack;
import io.papermc.paper.datapack.DatapackSource;

/** The packs the server knows, read from the datapack service; selecting a pack enables it there. */
public class PackRepository {
    private Map<String, Pack> available = new LinkedHashMap<>();
    private List<String> selected = new ArrayList<>();

    public PackRepository(RepositorySource... sources) {
        reload();
    }

    public static String displayPackList(Collection<Pack> packs) {
        return packs.stream().map(pack -> pack.getPackSource() == PackSource.BUILT_IN ? pack.getId() : pack.getId() + " (" + pack.getPackSource() + ")")
            .collect(Collectors.joining(", "));
    }

    public void reload() {
        Bukkit.getDatapackManager().refreshPacks();
        Map<String, Pack> found = new LinkedHashMap<>();
        List<String> enabled = new ArrayList<>();
        for (Datapack datapack : Bukkit.getDatapackManager().getPacks()) {
            found.put(datapack.getName(), toPack(datapack));
            if (datapack.isEnabled()) {
                enabled.add(datapack.getName());
            }
        }
        available = found;
        selected = enabled;
    }

    private static Pack toPack(Datapack datapack) {
        DatapackSource kind = datapack.getSource();
        PackSource source = kind == DatapackSource.BUILT_IN ? PackSource.BUILT_IN
            : kind == DatapackSource.FEATURE ? PackSource.FEATURE : PackSource.WORLD;
        PackCompatibility compatibility = switch (datapack.getCompatibility()) {
            case TOO_OLD -> PackCompatibility.TOO_OLD;
            case TOO_NEW -> PackCompatibility.TOO_NEW;
            default -> PackCompatibility.COMPATIBLE;
        };
        Component title = org.basaltmc.bukkit.facade.ChatJson.fromAdventure(datapack.getTitle());
        Component description = org.basaltmc.bukkit.facade.ChatJson.fromAdventure(datapack.getDescription());
        PackLocationInfo location = new PackLocationInfo(datapack.getName(), title, source, Optional.empty());
        Pack.Metadata metadata = new Pack.Metadata(description, compatibility, FeatureFlagSet.of(), List.of());
        return new Pack(location, new Pack.ResourcesSupplier() {
            public PackMetadataResources openMetadata(PackLocationInfo info) {
                throw new UnsupportedOperationException("Pack resources are read by the Basalt server, not through the facade");
            }

            public Stream<PackResources> openResources(PackLocationInfo info, Pack.Metadata meta) {
                return Stream.empty();
            }
        }, metadata, new PackSelectionConfig(datapack.isRequired(), Pack.Position.TOP, false));
    }

    public boolean isAbleToClearAnyPack() {
        return selected.stream().anyMatch(id -> !available.get(id).isRequired());
    }

    public void setSelected(Collection<String> packs) {
        List<String> next = new ArrayList<>();
        for (String id : packs) {
            if (available.containsKey(id)) {
                next.add(id);
            }
        }
        for (String id : available.keySet()) {
            if (available.get(id).isRequired() && !next.contains(id)) {
                next.add(id);
            }
        }
        for (String id : available.keySet()) {
            Datapack datapack = Bukkit.getDatapackManager().getPack(id);
            if (datapack != null && datapack.isEnabled() != next.contains(id)) {
                datapack.setEnabled(next.contains(id));
            }
        }
        selected = next;
    }

    public boolean addPack(String packId) {
        if (!available.containsKey(packId) || selected.contains(packId)) {
            return false;
        }
        List<String> next = new ArrayList<>(selected);
        next.add(packId);
        setSelected(next);
        return true;
    }

    public boolean removePack(String packId) {
        if (!selected.contains(packId)) {
            return false;
        }
        List<String> next = new ArrayList<>(selected);
        next.remove(packId);
        setSelected(next);
        return true;
    }

    public Collection<String> getAvailableIds() {
        return List.copyOf(available.keySet());
    }

    public Collection<Pack> getAvailablePacks() {
        return List.copyOf(available.values());
    }

    public Collection<String> getSelectedIds() {
        return List.copyOf(selected);
    }

    public FeatureFlagSet getRequestedFeatureFlags() {
        return FeatureFlagSet.of();
    }

    public Collection<Pack> getSelectedPacks() {
        return selected.stream().map(available::get).toList();
    }

    public Pack getPack(String id) {
        return available.get(id);
    }

    public boolean isAvailable(String id) {
        return available.containsKey(id);
    }

    public List<PackResources> openAllSelected() {
        return getSelectedPacks().stream().flatMap(Pack::open).toList();
    }
}
