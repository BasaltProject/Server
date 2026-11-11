// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.saveddata.maps.MapDecorationType;
import net.minecraft.world.level.saveddata.maps.MapDecorationTypes;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class ExplorationMapFunction extends LootItemConditionalFunction {
    public static final Holder<MapDecorationType> DEFAULT_DECORATION = MapDecorationTypes.WOODLAND_MANSION;
    public static final byte DEFAULT_ZOOM = 2;
    public static final int DEFAULT_SEARCH_RADIUS = 50;
    public static final boolean DEFAULT_SKIP_EXISTING = true;
    public static final MapCodec<ExplorationMapFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.ExplorationMapFunction.class, "MAP_CODEC");
    private final HolderSet<Structure> destination;
    private final Holder<MapDecorationType> mapDecoration;
    private final byte zoom;
    private final int searchRadius;
    private final boolean skipKnownStructures;
    private ExplorationMapFunction(Optional<Holder<LootItemCondition>> condition, HolderSet<Structure> destination, Holder<MapDecorationType> mapDecoration, byte zoom, int searchRadius, boolean skipKnownStructures) {
        super(condition);
        this.destination = destination;
        this.mapDecoration = mapDecoration;
        this.zoom = zoom;
        this.searchRadius = searchRadius;
        this.skipKnownStructures = skipKnownStructures;
    }
    protected void writeFunction(StringBuilder out) {
        out.append("\"type\":\"minecraft:exploration_map\",\"destination\":");
        org.basaltmc.bukkit.loot.LootJson.holderSet(out, destination);
        if (mapDecoration != null) {
            out.append(",\"decoration\":");
            org.basaltmc.bukkit.loot.LootJson.string(out, mapDecoration.getRegisteredName());
        }
        out.append(",\"zoom\":").append(zoom).append(",\"search_radius\":").append(searchRadius)
            .append(",\"skip_existing_chunks\":").append(skipKnownStructures);
    }
    public MapCodec<ExplorationMapFunction> codec() {
        return MAP_CODEC;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        return org.basaltmc.bukkit.loot.LootFunctionBridge.apply(this, itemStack, context);
    }
    public static Builder makeExplorationMap(HolderSet<Structure> destination) {
        return new Builder(destination);
    }
    public static class Builder extends LootItemConditionalFunction.Builder<Builder> {
        private final HolderSet<Structure> destination;
        private Holder<MapDecorationType> mapDecoration = DEFAULT_DECORATION;
        private byte zoom = DEFAULT_ZOOM;
        private int searchRadius = DEFAULT_SEARCH_RADIUS;
        private boolean skipKnownStructures = DEFAULT_SKIP_EXISTING;
        public Builder(HolderSet<Structure> destination) {
            this.destination = destination;
        }
        protected Builder getThis() {
            return this;
        }
        public Builder setMapDecoration(Holder<MapDecorationType> mapDecoration) {
            this.mapDecoration = mapDecoration;
            return this;
        }
        public Builder setZoom(byte zoom) {
            this.zoom = zoom;
            return this;
        }
        public Builder setSearchRadius(int searchRadius) {
            this.searchRadius = searchRadius;
            return this;
        }
        public Builder setSkipKnownStructures(boolean skipKnownStructures) {
            this.skipKnownStructures = skipKnownStructures;
            return this;
        }
        public LootItemFunction build() {
            return new ExplorationMapFunction(getCondition(), destination, mapDecoration, zoom, searchRadius, skipKnownStructures);
        }
    }
}
