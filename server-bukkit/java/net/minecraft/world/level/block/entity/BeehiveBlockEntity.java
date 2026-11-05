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
        private final Occupant occupant;
        private int ticksInHive;
        private BeeData(Occupant occupant) {
            this.occupant = occupant;
            this.ticksInHive = occupant.ticksInHive();
        }
        public boolean tick() {
            ticksInHive++;
            return ticksInHive > occupant.minTicksInHive();
        }
        public Occupant toOccupant() {
            return new Occupant(occupant.entityData(), ticksInHive, occupant.minTicksInHive());
        }
        public boolean hasNectar() {
            return occupant.entityData().getUnsafe().getBooleanOr("HasNectar", false);
        }
    }

    private static final int kMinTicksInHive = 600;
    private static final java.util.List<String> kIgnoredBeeTags = java.util.List.of("Air", "drop_chances", "equipment",
        "Brain", "CanPickUpLoot", "FallDistance", "FallFlying", "Fire", "HurtByTimestamp", "HurtTime", "LeftHanded",
        "Motion", "NoGravity", "OnGround", "PortalCooldown", "Pos", "Rotation", "sleeping_pos", "CannotEnterHiveTicks",
        "TicksSincePollination", "CropsGrownSincePollination", "hive_pos", "Passengers", "leash", "UUID", "id");

    public static record Occupant(TypedEntityData<EntityType<?>> entityData, int ticksInHive, int minTicksInHive) {
        public static final Codec<Occupant> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.entity.BeehiveBlockEntity.Occupant.class, "CODEC");
        public static final Codec<List<Occupant>> LIST_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.entity.BeehiveBlockEntity.Occupant.class, "LIST_CODEC");
        public static final StreamCodec<RegistryFriendlyByteBuf, Occupant> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.block.entity.BeehiveBlockEntity.Occupant.class, "STREAM_CODEC");
        public static Occupant of(Entity entity) {
            if (!(entity.level() instanceof ServerLevel server)) {
                return create(0);
            }
            byte[] raw = org.basaltmc.bukkit.WorldNatives.entityNbt(server.getWorld().getName(), entity.getId());
            CompoundTag tag = raw == null ? new CompoundTag() : org.basaltmc.bukkit.facade.NbtCodec.fromBytes(raw);
            for (String ignored : kIgnoredBeeTags) {
                tag.remove(ignored);
            }
            return new Occupant(TypedEntityData.of(entity.getType(), tag), 0, kMinTicksInHive);
        }
        public static Occupant create(int ticksInHive) {
            return new Occupant(TypedEntityData.<EntityType<?>>of(net.minecraft.core.registries.BuiltInRegistries.ENTITY_TYPE.getValue(net.minecraft.resources.Identifier.withDefaultNamespace("bee")), new CompoundTag()), ticksInHive, kMinTicksInHive);
        }
        public Entity createEntity(Level level, BlockPos hivePos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.entity.BeehiveBlockEntity.Occupant#createEntity");
        }
    }
}
