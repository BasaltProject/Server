// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderSet;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.level.material.FluidState;
import org.basaltmc.bukkit.facade.HolderSets;
import org.basaltmc.bukkit.facade.ReflectiveCodecs;
public record FluidPredicate(Optional<HolderSet<Fluid>> fluids, Optional<StatePropertiesPredicate> properties) {
    public static final Codec<FluidPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        ReflectiveCodecs.holderSet(Fluid.class).optionalFieldOf("fluids").forGetter(FluidPredicate::fluids),
        StatePropertiesPredicate.CODEC.optionalFieldOf("state").forGetter(FluidPredicate::properties)).apply(i, FluidPredicate::new));
    public boolean matches(ServerLevel level, BlockPos pos) {
        if (!level.isLoaded(pos)) {
            return false;
        }
        FluidState state = level.getFluidState(pos);
        if (fluids.isPresent() && !HolderSets.contains(fluids.get(), state.getType().builtInRegistryHolder())) {
            return false;
        }
        return properties.isEmpty() || properties.get().matches(state);
    }
    public static class Builder {
        private Optional<HolderSet<Fluid>> fluids = Optional.empty();
        private Optional<StatePropertiesPredicate> properties = Optional.empty();
        private Builder() {}
        public static Builder fluid() {
            return new Builder();
        }
        public Builder of(Fluid fluid) {
            fluids = Optional.of(HolderSet.direct(fluid.builtInRegistryHolder()));
            return this;
        }
        public Builder of(HolderSet<Fluid> set) {
            fluids = Optional.of(set);
            return this;
        }
        public Builder setProperties(StatePropertiesPredicate properties) {
            this.properties = Optional.of(properties);
            return this;
        }
        public FluidPredicate build() {
            return new FluidPredicate(fluids, properties);
        }
    }
}
