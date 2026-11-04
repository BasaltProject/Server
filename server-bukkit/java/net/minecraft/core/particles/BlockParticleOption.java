// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.particles;

import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;

public class BlockParticleOption implements ParticleOptions {
    private final ParticleType<BlockParticleOption> type;
    private final net.minecraft.world.level.block.state.BlockState state;

    public static MapCodec<BlockParticleOption> codec(ParticleType<BlockParticleOption> type) {
        return org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(BlockParticleOption.class, "CODEC");
    }

    public static StreamCodec<? super RegistryFriendlyByteBuf, BlockParticleOption> streamCodec(ParticleType<BlockParticleOption> type) {
        return StreamCodec.of((buf, options) -> buf.writeVarInt(net.minecraft.world.level.block.Block.getId(options.state)),
            buf -> new BlockParticleOption(type, net.minecraft.world.level.block.Block.stateById(buf.readVarInt())));
    }

    public BlockParticleOption(ParticleType<BlockParticleOption> type, net.minecraft.world.level.block.state.BlockState state) {
        this.type = type;
        this.state = state;
    }

    public ParticleType<BlockParticleOption> getType() {
        return type;
    }

    public net.minecraft.world.level.block.state.BlockState getState() {
        return state;
    }
}
