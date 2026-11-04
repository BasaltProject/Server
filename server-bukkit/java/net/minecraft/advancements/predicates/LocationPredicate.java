// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.levelgen.structure.Structure;
import org.basaltmc.bukkit.facade.HolderSets;
import org.basaltmc.bukkit.facade.ReflectiveCodecs;
import org.basaltmc.bukkit.facade.StructureLookup;
public record LocationPredicate(Optional<PositionPredicate> position, Optional<HolderSet<Biome>> biomes, Optional<HolderSet<Structure>> structures, Optional<ResourceKey<Level>> dimension, Optional<Boolean> smokey, Optional<LightPredicate> light, Optional<BlockPredicate> block, Optional<FluidPredicate> fluid, Optional<Boolean> canSeeSky) {
    public static final Codec<LocationPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        PositionPredicate.CODEC.optionalFieldOf("position").forGetter(LocationPredicate::position),
        ReflectiveCodecs.holderSet(Biome.class).optionalFieldOf("biomes").forGetter(LocationPredicate::biomes),
        ReflectiveCodecs.holderSet(Structure.class).optionalFieldOf("structures").forGetter(LocationPredicate::structures),
        ResourceKey.codec(net.minecraft.core.registries.Registries.DIMENSION).optionalFieldOf("dimension").forGetter(LocationPredicate::dimension),
        Codec.BOOL.optionalFieldOf("smokey").forGetter(LocationPredicate::smokey),
        LightPredicate.CODEC.optionalFieldOf("light").forGetter(LocationPredicate::light),
        BlockPredicate.CODEC.optionalFieldOf("block").forGetter(LocationPredicate::block),
        FluidPredicate.CODEC.optionalFieldOf("fluid").forGetter(LocationPredicate::fluid),
        Codec.BOOL.optionalFieldOf("can_see_sky").forGetter(LocationPredicate::canSeeSky)).apply(i, LocationPredicate::new));
    public boolean matches(ServerLevel level, double x, double y, double z) {
        if (position.isPresent() && !position.get().matches(x, y, z)) {
            return false;
        }
        if (dimension.isPresent() && dimension.get() != level.dimension()) {
            return false;
        }
        BlockPos pos = BlockPos.containing(x, y, z);
        boolean loaded = level.isLoaded(pos);
        if (biomes.isPresent() && !(loaded && HolderSets.contains(biomes.get(), level.getBiome(pos)))) {
            return false;
        }
        if (structures.isPresent() && !(loaded && StructureLookup.inAny(level, pos, structures.get()))) {
            return false;
        }
        if (smokey.isPresent() && !(loaded && smokey.get() == net.minecraft.world.level.block.CampfireBlock.isSmokeyPos(level, pos))) {
            return false;
        }
        if (light.isPresent() && !light.get().matches(level, pos)) {
            return false;
        }
        if (block.isPresent() && !block.get().matches(level, pos)) {
            return false;
        }
        if (fluid.isPresent() && !fluid.get().matches(level, pos)) {
            return false;
        }
        return canSeeSky.isEmpty() || canSeeSky.get() == level.canSeeSky(pos);
    }
    public record PositionPredicate(MinMaxBounds.Doubles x, MinMaxBounds.Doubles y, MinMaxBounds.Doubles z) {
        public static final Codec<PositionPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
            MinMaxBounds.Doubles.CODEC.optionalFieldOf("x", MinMaxBounds.Doubles.ANY).forGetter(PositionPredicate::x),
            MinMaxBounds.Doubles.CODEC.optionalFieldOf("y", MinMaxBounds.Doubles.ANY).forGetter(PositionPredicate::y),
            MinMaxBounds.Doubles.CODEC.optionalFieldOf("z", MinMaxBounds.Doubles.ANY).forGetter(PositionPredicate::z)).apply(i, PositionPredicate::new));
        private static Optional<PositionPredicate> of(MinMaxBounds.Doubles x, MinMaxBounds.Doubles y, MinMaxBounds.Doubles z) {
            return x.isAny() && y.isAny() && z.isAny() ? Optional.empty() : Optional.of(new PositionPredicate(x, y, z));
        }
        public boolean matches(double px, double py, double pz) {
            return x.matches(px) && y.matches(py) && z.matches(pz);
        }
    }
    public static class Builder {
        private MinMaxBounds.Doubles x = MinMaxBounds.Doubles.ANY;
        private MinMaxBounds.Doubles y = MinMaxBounds.Doubles.ANY;
        private MinMaxBounds.Doubles z = MinMaxBounds.Doubles.ANY;
        private Optional<HolderSet<Biome>> biomes = Optional.empty();
        private Optional<HolderSet<Structure>> structures = Optional.empty();
        private Optional<ResourceKey<Level>> dimension = Optional.empty();
        private Optional<Boolean> smokey = Optional.empty();
        private Optional<LightPredicate> light = Optional.empty();
        private Optional<BlockPredicate> block = Optional.empty();
        private Optional<FluidPredicate> fluid = Optional.empty();
        private Optional<Boolean> canSeeSky = Optional.empty();
        public static Builder location() {
            return new Builder();
        }
        public static Builder inBiome(Holder<Biome> biome) {
            return location().setBiomes(HolderSet.direct(biome));
        }
        public static Builder inDimension(ResourceKey<Level> dimension) {
            return location().setDimension(dimension);
        }
        public static Builder inStructure(Holder<Structure> structure) {
            return location().setStructures(HolderSet.direct(structure));
        }
        public static Builder atYLocation(MinMaxBounds.Doubles yLocation) {
            return location().setY(yLocation);
        }
        public Builder setX(MinMaxBounds.Doubles x) {
            this.x = x;
            return this;
        }
        public Builder setY(MinMaxBounds.Doubles y) {
            this.y = y;
            return this;
        }
        public Builder setZ(MinMaxBounds.Doubles z) {
            this.z = z;
            return this;
        }
        public Builder setBiomes(HolderSet<Biome> biomes) {
            this.biomes = Optional.of(biomes);
            return this;
        }
        public Builder setStructures(HolderSet<Structure> structures) {
            this.structures = Optional.of(structures);
            return this;
        }
        public Builder setDimension(ResourceKey<Level> dimension) {
            this.dimension = Optional.of(dimension);
            return this;
        }
        public Builder setLight(LightPredicate.Builder light) {
            this.light = Optional.of(light.build());
            return this;
        }
        public Builder setBlock(BlockPredicate.Builder block) {
            this.block = Optional.of(block.build());
            return this;
        }
        public Builder setFluid(FluidPredicate.Builder fluid) {
            this.fluid = Optional.of(fluid.build());
            return this;
        }
        public Builder setSmokey(boolean smokey) {
            this.smokey = Optional.of(smokey);
            return this;
        }
        public Builder setCanSeeSky(boolean canSeeSky) {
            this.canSeeSky = Optional.of(canSeeSky);
            return this;
        }
        public LocationPredicate build() {
            return new LocationPredicate(PositionPredicate.of(x, y, z), biomes, structures, dimension, smokey, light, block, fluid, canSeeSky);
        }
    }
}
