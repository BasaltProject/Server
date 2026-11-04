// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import com.mojang.serialization.Codec;
import java.util.List;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentMap;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.debug.DebugValueSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.animal.bee.Bee;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.component.TypedEntityData;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;

import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.facade.TileNbt;

public class BeehiveBlockEntity extends BlockEntity {
    public BeehiveBlockEntity(BlockPos worldPosition, BlockState blockState) {
        super(BlockEntityType.named("beehive"), worldPosition, blockState);
    }

    public BeehiveBlockEntity(ServerLevel level, BlockPos pos, CompoundTag nbt) {
        super(level, pos, nbt);
    }

    public static final int MAX_OCCUPANTS = 3;

    public boolean isFull() {
        return getOccupantCount() >= MAX_OCCUPANTS;
    }

    public boolean isSedated() {
        org.bukkit.block.Beehive hive = bukkit(org.bukkit.block.Beehive.class);
        return hive != null && hive.isSedated();
    }

    public void emptyAllLivingFromHive(Player player, BeeReleaseStatus releaseStatus) {
        org.bukkit.block.Beehive hive = bukkit(org.bukkit.block.Beehive.class);
        if (hive != null) {
            hive.releaseEntities();
        }
    }

    public int getHoneyLevel() {
        return getBlockState().getValue(net.minecraft.world.level.block.state.properties.BlockStateProperties.LEVEL_HONEY);
    }

    public boolean isEmpty() {
        return nbt().getListOrEmpty("bees").isEmpty();
    }

    public int getOccupantCount() {
        return nbt().getListOrEmpty("bees").size();
    }

    public boolean hasSavedFlowerPos() {
        return nbt().contains("flower_pos");
    }

    public static enum BeeReleaseStatus {
        HONEY_DELIVERED, BEE_RELEASED, EMERGENCY;
    }

    static class BeeData {
        private BeeData(Occupant occupant) {
            
        }
        public boolean tick() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.entity.BeehiveBlockEntity.BeeData#tick");
        }
        public Occupant toOccupant() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.entity.BeehiveBlockEntity.BeeData#toOccupant");
        }
        public boolean hasNectar() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.entity.BeehiveBlockEntity.BeeData#hasNectar");
        }
    }

    public static record Occupant(TypedEntityData<EntityType<?>> entityData, int ticksInHive, int minTicksInHive) {
        public static final Codec<Occupant> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.entity.BeehiveBlockEntity.Occupant.class, "CODEC");
        public static final Codec<List<Occupant>> LIST_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.entity.BeehiveBlockEntity.Occupant.class, "LIST_CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, Occupant> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.block.entity.BeehiveBlockEntity.Occupant.class, "STREAM_CODEC");
        public static Occupant of(Entity entity) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.entity.BeehiveBlockEntity.Occupant#of");
        }
        public static Occupant create(int ticksInHive) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.entity.BeehiveBlockEntity.Occupant#create");
        }
        public Entity createEntity(Level level, BlockPos hivePos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.entity.BeehiveBlockEntity.Occupant#createEntity");
        }
    }
}
