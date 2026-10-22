package net.minecraft.world.level.chunk;

import net.minecraft.core.Holder;
import net.minecraft.core.RegistryAccess;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.Unsupported;
public record PalettedContainerFactory(Strategy blockStatesStrategy, BlockState defaultBlockState, Object blockStatesContainerCodec, Strategy biomeStrategy, Holder defaultBiome, Object biomeContainerCodec) {
    public static PalettedContainerFactory create(RegistryAccess registries) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.PalettedContainerFactory.create");
    }
    public PalettedContainer createForBiomes() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.PalettedContainerFactory.createForBiomes");
    }
    public PalettedContainer createForBlockStates() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.PalettedContainerFactory.createForBlockStates");
    }
}
