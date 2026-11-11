// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.decoration.painting;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.PaintingVariantTags;
import net.minecraft.util.Util;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.world.level.gamerules.GameRules;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.Items;
import net.minecraft.core.component.DataComponents;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.game.ClientGamePacketListener;
import net.minecraft.network.protocol.game.ClientboundAddEntityPacket;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerEntity;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.decoration.HangingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
public class Painting extends HangingEntity {
    public static final float DEPTH = 0.0625F;
    public Painting(EntityType<? extends Painting> type, Level level) {
        super(type, level);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    @SuppressWarnings("removal")
    public Holder<PaintingVariant> getVariant() {
        if (live() instanceof org.bukkit.entity.Painting art) {
            Identifier id = Identifier.parse(art.getArt().getKey().toString());
            Holder<PaintingVariant> found = lookup(ResourceKey.create(Registries.PAINTING_VARIANT, id));
            if (found != null) {
                return found;
            }
        }
        return variant;
    }
    public <T> T get(DataComponentType<? extends T> type) {
        return super.get(type);
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        return false;
    }
    public static Optional<Painting> create(Level level, BlockPos pos, Direction direction) {
        Painting candidate = new Painting(level, pos);
        List<Holder<PaintingVariant>> options = new ArrayList<>();
        level.registryAccess().lookupOrThrow(Registries.PAINTING_VARIANT).getTagOrEmpty(PaintingVariantTags.PLACEABLE).forEach(options::add);
        if (options.isEmpty()) {
            return Optional.empty();
        }
        candidate.setDirection(direction);
        options.removeIf(option -> {
            candidate.variant = option;
            return !candidate.survives();
        });
        if (options.isEmpty()) {
            return Optional.empty();
        }
        int largest = options.stream().mapToInt(option -> option.value().area()).max().orElse(0);
        options.removeIf(option -> option.value().area() < largest);
        Optional<Holder<PaintingVariant>> chosen = Util.getRandomSafe(options, candidate.random);
        if (chosen.isEmpty()) {
            return Optional.empty();
        }
        candidate.variant = chosen.get();
        candidate.setDirection(direction);
        return Optional.of(candidate);
    }
    private Painting(Level level, BlockPos blockPos) {
        super(EntityTypes.PAINTING, level, blockPos);
    }
    public Painting(Level level, BlockPos blockPos, Direction direction, Holder<PaintingVariant> variant) {
        this(level, blockPos);
        this.variant = variant;
        setDirection(direction);
    }
    private Holder<PaintingVariant> variant;
    @SuppressWarnings({"unchecked", "rawtypes"})
    private Holder<PaintingVariant> lookup(ResourceKey key) {
        return (Holder<PaintingVariant>) level().registryAccess().lookupOrThrow((ResourceKey) Registries.PAINTING_VARIANT).get(key).orElse(null);
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected AABB calculateBoundingBox(BlockPos pos, Direction direction) {
        Vec3 wall = Vec3.atCenterOf(pos).relative(direction, -0.46875);
        PaintingVariant shape = getVariant().value();
        double across = (shape.width() % 2 == 0 ? 0.5 : 0.0);
        double up = (shape.height() % 2 == 0 ? 0.5 : 0.0);
        Vec3 center = wall.relative(direction.getCounterClockWise(), across).relative(Direction.UP, up);
        Direction.Axis axis = direction.getAxis();
        return AABB.ofSize(center, axis == Direction.Axis.X ? 0.0625 : shape.width(), shape.height(), axis == Direction.Axis.Z ? 0.0625 : shape.width());
    }
    public void dropItem(ServerLevel level, Entity causedBy) {
        if (level.getGameRules().get(GameRules.ENTITY_DROPS)) {
            playSound(SoundEvents.PAINTING_BREAK, 1.0F, 1.0F);
            if (causedBy instanceof Player player && player.hasInfiniteMaterials()) {
                return;
            }
            ItemStack drop = new ItemStack(Items.PAINTING);
            drop.set(DataComponents.CUSTOM_NAME, getCustomName());
            spawnAtLocation(level, drop);
        }
    }
    public void playPlacementSound() {
        playSound(SoundEvents.PAINTING_PLACE, 1.0F, 1.0F);
    }
    public void snapTo(double x, double y, double z, float yRot, float xRot) {
        setPos(x, y, z);
    }
    public Vec3 trackingPosition() {
        return Vec3.atLowerCornerOf(getPos());
    }
    public Packet<ClientGamePacketListener> getAddEntityPacket(ServerEntity serverEntity) {
        return new ClientboundAddEntityPacket(this, getDirection().get3DDataValue(), getPos());
    }
    public void recreateFromPacket(ClientboundAddEntityPacket packet) {
        super.recreateFromPacket(packet);
    }
    public ItemStack getPickResult() {
        return new ItemStack(Items.PAINTING);
    }
}
