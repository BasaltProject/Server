// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.syncher;
import io.netty.buffer.ByteBuf;
import java.util.function.BiConsumer;
import java.util.function.Function;
import org.basaltmc.bukkit.net.PacketCodec;
import java.util.List;
import java.util.Optional;
import java.util.OptionalInt;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.GlobalPos;
import net.minecraft.core.Holder;
import net.minecraft.core.Rotations;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.network.chat.Component;
import net.minecraft.world.entity.EntityReference;
import net.minecraft.world.entity.HumanoidArm;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.animal.armadillo.Armadillo;
import net.minecraft.world.entity.animal.chicken.ChickenSoundVariant;
import net.minecraft.world.entity.animal.chicken.ChickenVariant;
import net.minecraft.world.entity.animal.cow.CowSoundVariant;
import net.minecraft.world.entity.animal.cow.CowVariant;
import net.minecraft.world.entity.animal.feline.CatSoundVariant;
import net.minecraft.world.entity.animal.feline.CatVariant;
import net.minecraft.world.entity.animal.frog.FrogVariant;
import net.minecraft.world.entity.animal.golem.CopperGolemState;
import net.minecraft.world.entity.animal.nautilus.ZombieNautilusVariant;
import net.minecraft.world.entity.animal.pig.PigSoundVariant;
import net.minecraft.world.entity.animal.pig.PigVariant;
import net.minecraft.world.entity.animal.sniffer.Sniffer;
import net.minecraft.world.entity.animal.wolf.WolfSoundVariant;
import net.minecraft.world.entity.animal.wolf.WolfVariant;
import net.minecraft.world.entity.decoration.painting.PaintingVariant;
import net.minecraft.world.entity.npc.villager.VillagerData;
import net.minecraft.world.item.DyeColor;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.component.ResolvableProfile;
import net.minecraft.world.level.block.WeatheringCopper;
import net.minecraft.world.level.block.state.BlockState;
import org.joml.Quaternionfc;
import org.joml.Vector3fc;
public final class EntityDataSerializers {
    private EntityDataSerializers() {}
    private static <T> EntityDataSerializer<T> serializer(
        int id, BiConsumer<ByteBuf, T> writer, Function<ByteBuf, T> reader) {
        return new EntityDataSerializer<>() {
            public int id() {
                return id;
            }
            public void write(ByteBuf out, T value) {
                writer.accept(out, value);
            }
            public T read(ByteBuf in) {
                return reader.apply(in);
            }
        };
    }
    public static final EntityDataSerializer<Byte> BYTE =
        serializer(0, (out, v) -> out.writeByte(v), ByteBuf::readByte);
    public static final EntityDataSerializer<Integer> INT = serializer(1, PacketCodec::varInt, PacketCodec::readVarInt);
    public static final EntityDataSerializer<Long> LONG = serializer(2, PacketCodec::varLong, PacketCodec::readVarLong);
    public static final EntityDataSerializer<Float> FLOAT =
        serializer(3, (out, v) -> out.writeFloat(v), ByteBuf::readFloat);
    public static final EntityDataSerializer<String> STRING =
        serializer(4, PacketCodec::string, PacketCodec::readString);
    public static final EntityDataSerializer<Boolean> BOOLEAN =
        serializer(8, (out, v) -> out.writeBoolean(v), ByteBuf::readBoolean);
    private static <T> EntityDataSerializer<Holder<T>> variants(int id, net.minecraft.resources.ResourceKey<? extends net.minecraft.core.Registry<T>> registry, Class<T> type, io.papermc.paper.registry.RegistryKey<? extends org.bukkit.Keyed> key) {
        return serializer(id, (out, holder) -> PacketCodec.varInt(out, org.basaltmc.bukkit.facade.VariantHolders.indexOf(key, holder)),
            in -> org.basaltmc.bukkit.facade.VariantHolders.at(registry, type, key, PacketCodec.readVarInt(in)));
    }
    public static EntityDataSerializer<?> getSerializer(int id) {
        return switch (id) {
            case 0 -> BYTE;
            case 1 -> INT;
            case 2 -> LONG;
            case 3 -> FLOAT;
            case 4 -> STRING;
            case 5 -> COMPONENT;
            case 6 -> OPTIONAL_COMPONENT;
            case 7 -> ITEM_STACK;
            case 8 -> BOOLEAN;
            case 9 -> ROTATIONS;
            case 10 -> BLOCK_POS;
            case 11 -> OPTIONAL_BLOCK_POS;
            case 12 -> DIRECTION;
            case 13 -> OPTIONAL_LIVING_ENTITY_REFERENCE;
            case 14 -> BLOCK_STATE;
            case 15 -> OPTIONAL_BLOCK_STATE;
            case 16 -> PARTICLE;
            case 17 -> PARTICLES;
            case 18 -> VILLAGER_DATA;
            case 19 -> OPTIONAL_UNSIGNED_INT;
            case 20 -> POSE;
            case 21 -> CAT_VARIANT;
            case 22 -> CAT_SOUND_VARIANT;
            case 23 -> COW_VARIANT;
            case 24 -> COW_SOUND_VARIANT;
            case 25 -> WOLF_VARIANT;
            case 26 -> WOLF_SOUND_VARIANT;
            case 27 -> FROG_VARIANT;
            case 28 -> PIG_VARIANT;
            case 29 -> PIG_SOUND_VARIANT;
            case 30 -> CHICKEN_VARIANT;
            case 31 -> CHICKEN_SOUND_VARIANT;
            case 32 -> ZOMBIE_NAUTILUS_VARIANT;
            case 34 -> PAINTING_VARIANT;
            case 41 -> RESOLVABLE_PROFILE;
            case 33 -> OPTIONAL_GLOBAL_POS;
            case 35 -> SNIFFER_STATE;
            case 36 -> ARMADILLO_STATE;
            case 37 -> COPPER_GOLEM_STATE;
            case 38 -> WEATHERING_COPPER_STATE;
            case 39 -> VECTOR3;
            case 40 -> QUATERNION;
            case 42 -> HUMANOID_ARM;
            case 43 -> DYE_COLOR;
            default -> null;
        };
    }
    public static int getSerializedId(EntityDataSerializer<?> serializer) {
        return serializer.id();
    }
    private static <T> EntityDataSerializer<T> viaCodec(int id, net.minecraft.network.codec.StreamCodec<? super net.minecraft.network.RegistryFriendlyByteBuf, T> codec) {
        return serializer(id, (out, v) -> codec.encode(new net.minecraft.network.RegistryFriendlyByteBuf(out, null), v),
            in -> codec.decode(new net.minecraft.network.RegistryFriendlyByteBuf(in, null)));
    }
    public static final EntityDataSerializer<Component> COMPONENT =
        viaCodec(5, net.minecraft.network.chat.ComponentSerialization.STREAM_CODEC);
    public static final EntityDataSerializer<Optional<Component>> OPTIONAL_COMPONENT =
        viaCodec(6, net.minecraft.network.chat.ComponentSerialization.OPTIONAL_STREAM_CODEC);
    public static final EntityDataSerializer<ItemStack> ITEM_STACK = viaCodec(7, ItemStack.OPTIONAL_STREAM_CODEC);
    public static final EntityDataSerializer<BlockState> BLOCK_STATE =
        serializer(14, (out, v) -> PacketCodec.varInt(out, net.minecraft.world.level.block.Block.getId(v)),
            in -> net.minecraft.world.level.block.Block.stateById(PacketCodec.readVarInt(in)));
    public static final EntityDataSerializer<Optional<BlockState>> OPTIONAL_BLOCK_STATE =
        serializer(15, (out, v) -> PacketCodec.varInt(out, v.map(net.minecraft.world.level.block.Block::getId).orElse(0)),
            in -> {int raw = PacketCodec.readVarInt(in); return raw == 0 ? Optional.empty() : Optional.of(net.minecraft.world.level.block.Block.stateById(raw));});
    public static final EntityDataSerializer<ParticleOptions> PARTICLE =
        viaCodec(16, net.minecraft.core.particles.ParticleTypes.STREAM_CODEC);
    public static final EntityDataSerializer<List<ParticleOptions>> PARTICLES =
        viaCodec(17, net.minecraft.core.particles.ParticleTypes.STREAM_CODEC.apply(net.minecraft.network.codec.ByteBufCodecs.list()));
    public static final EntityDataSerializer<Rotations> ROTATIONS =
        serializer(9, (out, v) -> {out.writeFloat(v.x()); out.writeFloat(v.y()); out.writeFloat(v.z());}, in -> new Rotations(in.readFloat(), in.readFloat(), in.readFloat()));
    public static final EntityDataSerializer<BlockPos> BLOCK_POS =
        serializer(10, (out, v) -> out.writeLong(v.asLong()), in -> BlockPos.of(in.readLong()));
    public static final EntityDataSerializer<Optional<BlockPos>> OPTIONAL_BLOCK_POS =
        serializer(11, (out, v) -> {out.writeBoolean(v.isPresent()); v.ifPresent(pos -> out.writeLong(pos.asLong()));}, in -> in.readBoolean() ? Optional.of(BlockPos.of(in.readLong())) : Optional.empty());
    public static final EntityDataSerializer<Direction> DIRECTION =
        serializer(12, (out, v) -> PacketCodec.varInt(out, v.get3DDataValue()), in -> Direction.from3DDataValue(PacketCodec.readVarInt(in)));
    public static final EntityDataSerializer<Optional<EntityReference<LivingEntity>>> OPTIONAL_LIVING_ENTITY_REFERENCE =
        serializer(13, (out, v) -> {out.writeBoolean(v.isPresent()); v.ifPresent(ref -> {out.writeLong(ref.getUUID().getMostSignificantBits()); out.writeLong(ref.getUUID().getLeastSignificantBits());});},
            in -> in.readBoolean() ? Optional.of(EntityReference.<LivingEntity>of(new java.util.UUID(in.readLong(), in.readLong()))) : Optional.empty());
    public static final EntityDataSerializer<Optional<GlobalPos>> OPTIONAL_GLOBAL_POS =
        serializer(33, (out, v) -> {out.writeBoolean(v.isPresent()); v.ifPresent(pos -> {PacketCodec.string(out, pos.dimension().identifier().toString()); out.writeLong(pos.pos().asLong());});},
            in -> in.readBoolean() ? Optional.of(new GlobalPos(net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.DIMENSION,
                net.minecraft.resources.Identifier.parse(PacketCodec.readString(in))), BlockPos.of(in.readLong()))) : Optional.empty());
    public static final EntityDataSerializer<VillagerData> VILLAGER_DATA =
        serializer(18, (out, v) -> {
            PacketCodec.varInt(out, org.basaltmc.bukkit.facade.VariantHolders.indexOf(io.papermc.paper.registry.RegistryKey.VILLAGER_TYPE, v.type()));
            PacketCodec.varInt(out, org.basaltmc.bukkit.facade.VariantHolders.indexOf(io.papermc.paper.registry.RegistryKey.VILLAGER_PROFESSION, v.profession()));
            PacketCodec.varInt(out, v.level());
        }, in -> new VillagerData(
            org.basaltmc.bukkit.facade.VariantHolders.at(net.minecraft.core.registries.Registries.VILLAGER_TYPE, net.minecraft.world.entity.npc.villager.VillagerType.class,
                io.papermc.paper.registry.RegistryKey.VILLAGER_TYPE, PacketCodec.readVarInt(in)),
            org.basaltmc.bukkit.facade.VariantHolders.at(net.minecraft.core.registries.Registries.VILLAGER_PROFESSION, net.minecraft.world.entity.npc.villager.VillagerProfession.class,
                io.papermc.paper.registry.RegistryKey.VILLAGER_PROFESSION, PacketCodec.readVarInt(in)),
            PacketCodec.readVarInt(in)));
    public static final EntityDataSerializer<OptionalInt> OPTIONAL_UNSIGNED_INT =
        serializer(19, (out, v) -> PacketCodec.varInt(out, v.isPresent() ? v.getAsInt() + 1 : 0), in -> {int raw = PacketCodec.readVarInt(in); return raw == 0 ? OptionalInt.empty() : OptionalInt.of(raw - 1);});
    public static final EntityDataSerializer<Pose> POSE =
        serializer(20, (out, v) -> PacketCodec.varInt(out, v.ordinal()), in -> Pose.values()[PacketCodec.readVarInt(in)]);
    public static final EntityDataSerializer<Holder<CatVariant>> CAT_VARIANT =
        variants(21, net.minecraft.core.registries.Registries.CAT_VARIANT, CatVariant.class, io.papermc.paper.registry.RegistryKey.CAT_VARIANT);
    public static final EntityDataSerializer<Holder<CatSoundVariant>> CAT_SOUND_VARIANT =
        variants(22, net.minecraft.core.registries.Registries.CAT_SOUND_VARIANT, CatSoundVariant.class, io.papermc.paper.registry.RegistryKey.CAT_SOUND_VARIANT);
    public static final EntityDataSerializer<Holder<ChickenVariant>> CHICKEN_VARIANT =
        variants(30, net.minecraft.core.registries.Registries.CHICKEN_VARIANT, ChickenVariant.class, io.papermc.paper.registry.RegistryKey.CHICKEN_VARIANT);
    public static final EntityDataSerializer<Holder<ChickenSoundVariant>> CHICKEN_SOUND_VARIANT =
        variants(31, net.minecraft.core.registries.Registries.CHICKEN_SOUND_VARIANT, ChickenSoundVariant.class, io.papermc.paper.registry.RegistryKey.CHICKEN_SOUND_VARIANT);
    public static final EntityDataSerializer<Holder<CowVariant>> COW_VARIANT =
        variants(23, net.minecraft.core.registries.Registries.COW_VARIANT, CowVariant.class, io.papermc.paper.registry.RegistryKey.COW_VARIANT);
    public static final EntityDataSerializer<Holder<CowSoundVariant>> COW_SOUND_VARIANT =
        variants(24, net.minecraft.core.registries.Registries.COW_SOUND_VARIANT, CowSoundVariant.class, io.papermc.paper.registry.RegistryKey.COW_SOUND_VARIANT);
    public static final EntityDataSerializer<Holder<WolfVariant>> WOLF_VARIANT =
        variants(25, net.minecraft.core.registries.Registries.WOLF_VARIANT, WolfVariant.class, io.papermc.paper.registry.RegistryKey.WOLF_VARIANT);
    public static final EntityDataSerializer<Holder<WolfSoundVariant>> WOLF_SOUND_VARIANT =
        variants(26, net.minecraft.core.registries.Registries.WOLF_SOUND_VARIANT, WolfSoundVariant.class, io.papermc.paper.registry.RegistryKey.WOLF_SOUND_VARIANT);
    public static final EntityDataSerializer<Holder<FrogVariant>> FROG_VARIANT =
        variants(27, net.minecraft.core.registries.Registries.FROG_VARIANT, FrogVariant.class, io.papermc.paper.registry.RegistryKey.FROG_VARIANT);
    public static final EntityDataSerializer<Holder<PigVariant>> PIG_VARIANT =
        variants(28, net.minecraft.core.registries.Registries.PIG_VARIANT, PigVariant.class, io.papermc.paper.registry.RegistryKey.PIG_VARIANT);
    public static final EntityDataSerializer<Holder<PigSoundVariant>> PIG_SOUND_VARIANT =
        variants(29, net.minecraft.core.registries.Registries.PIG_SOUND_VARIANT, PigSoundVariant.class, io.papermc.paper.registry.RegistryKey.PIG_SOUND_VARIANT);
    public static final EntityDataSerializer<Holder<ZombieNautilusVariant>> ZOMBIE_NAUTILUS_VARIANT =
        variants(32, net.minecraft.core.registries.Registries.ZOMBIE_NAUTILUS_VARIANT, ZombieNautilusVariant.class, io.papermc.paper.registry.RegistryKey.ZOMBIE_NAUTILUS_VARIANT);
    public static final EntityDataSerializer<Holder<PaintingVariant>> PAINTING_VARIANT =
        variants(34, net.minecraft.core.registries.Registries.PAINTING_VARIANT, PaintingVariant.class, io.papermc.paper.registry.RegistryKey.PAINTING_VARIANT);
    public static final EntityDataSerializer<Armadillo.ArmadilloState> ARMADILLO_STATE =
        serializer(36, (out, v) -> PacketCodec.varInt(out, v.ordinal()), in -> Armadillo.ArmadilloState.values()[PacketCodec.readVarInt(in)]);
    public static final EntityDataSerializer<Sniffer.State> SNIFFER_STATE =
        serializer(35, (out, v) -> PacketCodec.varInt(out, v.ordinal()), in -> Sniffer.State.values()[PacketCodec.readVarInt(in)]);
    public static final EntityDataSerializer<WeatheringCopper.WeatherState> WEATHERING_COPPER_STATE =
        serializer(38, (out, v) -> PacketCodec.varInt(out, v.ordinal()), in -> WeatheringCopper.WeatherState.values()[PacketCodec.readVarInt(in)]);
    public static final EntityDataSerializer<CopperGolemState> COPPER_GOLEM_STATE =
        serializer(37, (out, v) -> PacketCodec.varInt(out, v.ordinal()), in -> CopperGolemState.values()[PacketCodec.readVarInt(in)]);
    public static final EntityDataSerializer<Vector3fc> VECTOR3 =
        serializer(39, (out, v) -> {out.writeFloat(v.x()); out.writeFloat(v.y()); out.writeFloat(v.z());}, in -> new org.joml.Vector3f(in.readFloat(), in.readFloat(), in.readFloat()));
    public static final EntityDataSerializer<Quaternionfc> QUATERNION =
        serializer(40, (out, v) -> {out.writeFloat(v.x()); out.writeFloat(v.y()); out.writeFloat(v.z()); out.writeFloat(v.w());}, in -> new org.joml.Quaternionf(in.readFloat(), in.readFloat(), in.readFloat(), in.readFloat()));
    public static final EntityDataSerializer<ResolvableProfile> RESOLVABLE_PROFILE =
        viaCodec(41, net.minecraft.world.item.component.ResolvableProfile.STREAM_CODEC);
    public static final EntityDataSerializer<HumanoidArm> HUMANOID_ARM =
        serializer(42, (out, v) -> PacketCodec.varInt(out, v.ordinal()), in -> HumanoidArm.values()[PacketCodec.readVarInt(in)]);
    public static final EntityDataSerializer<DyeColor> DYE_COLOR =
        serializer(43, (out, v) -> PacketCodec.varInt(out, v.ordinal()), in -> DyeColor.values()[PacketCodec.readVarInt(in)]);
    public static void registerSerializer(EntityDataSerializer<?> serializer) {
        if (serializer == null) {
            throw new IllegalArgumentException("Serializer is not initialised");
        }
    }
}
