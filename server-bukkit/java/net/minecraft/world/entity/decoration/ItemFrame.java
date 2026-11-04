// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.decoration;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.game.ClientGamePacketListener;
import net.minecraft.network.protocol.game.ClientboundAddEntityPacket;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerEntity;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.MoverType;
import net.minecraft.world.entity.SlotAccess;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.saveddata.maps.MapId;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
public class ItemFrame extends HangingEntity {
    private org.bukkit.entity.ItemFrame frame() {
        return live() instanceof org.bukkit.entity.ItemFrame frame ? frame : null;
    }

    public static final int NUM_ROTATIONS = 8;
    public ItemFrame(EntityType<? extends ItemFrame> type, Level level) {
        super(type, level);
    }
    public ItemFrame(Level level, BlockPos pos, Direction direction) {
        this(net.minecraft.world.entity.EntityTypes.ITEM_FRAME, level, pos, direction);
    }
    public ItemFrame(EntityType<? extends ItemFrame> type, Level level, BlockPos pos, Direction direction) {
        super(type, level, pos);
        setDirection(direction);
        setInvisible(false);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {

    }
    protected void setDirection(Direction direction) {
        super.setDirectionRaw(direction);
        if (direction.getAxis().isHorizontal()) {
            setXRot(0.0F);
            setYRot(direction.get2DDataValue() * 90);
        } else {
            setXRot(-90 * direction.getAxisDirection().getStep());
            setYRot(0.0F);
        }
        recalculateBoundingBox();
    }
    protected final void recalculateBoundingBox() {
        super.recalculateBoundingBox();
    }
    protected AABB calculateBoundingBox(BlockPos blockPos, Direction direction) {
        return boxFor(blockPos, direction, hasFramedMap());
    }
    protected AABB getPopBox() {
        return boxFor(pos, getDirection(), false);
    }
    public boolean survives() {
        return true;
    }
    public void push(double xa, double ya, double za) {
        return;
    }
    protected void onKilled() {
        return;
    }
    public SoundEvent getRemoveItemSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.item_frame.remove_item");
    }
    public boolean shouldRenderAtSqrDistance(double distance) {
        return true;
    }
    public void dropItem(ServerLevel level, Entity causedBy) {
        playSound(getBreakSound(), 1.0F, 1.0F);
        ItemStack held = getItem();
        setItem(ItemStack.EMPTY);
        boolean creative = causedBy instanceof Player player && player.hasInfiniteMaterials();
        if (!creative && level.getGameRules().get(net.minecraft.world.level.gamerules.GameRules.ENTITY_DROPS)) {
            if (!held.isEmpty()) {
                spawnAtLocation(level, held.copy());
            }
            spawnAtLocation(level, getFrameItemStack());
        }
    }
    public SoundEvent getBreakSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.item_frame.break");
    }
    public void playPlacementSound() {
        playSound(getPlaceSound(), 1.0F, 1.0F);
    }
    public SoundEvent getPlaceSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.item_frame.place");
    }
    public ItemStack getItem() {
        org.bukkit.entity.ItemFrame frame = frame();
        return frame == null ? ItemStack.EMPTY : org.bukkit.craftbukkit.inventory.CraftItemStack.asNMSCopy(frame.getItem());
    }
    public MapId getFramedMapId(ItemStack itemStack) {
        return itemStack.get(net.minecraft.core.component.DataComponents.MAP_ID);
    }
    public boolean hasFramedMap() {
        return getItem().is(net.minecraft.world.item.Items.FILLED_MAP);
    }
    public void setItem(ItemStack itemStack) {
        setItem(itemStack, true);
    }
    public void setItem(ItemStack itemStack, boolean updateNeighbours) {
        org.bukkit.entity.ItemFrame frame = frame();
        if (frame != null) {
            frame.setItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack), updateNeighbours);
        }
    }
    public SoundEvent getAddItemSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.item_frame.add_item");
    }
    public SlotAccess getSlot(int slot) {
        return super.getSlot(slot);
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {

    }
    public int getRotation() {
        org.bukkit.entity.ItemFrame frame = frame();
        return frame == null ? 0 : frame.getRotation().ordinal();
    }
    public void setRotation(int rotation) {
        org.bukkit.entity.ItemFrame frame = frame();
        if (frame != null) {
            frame.setRotation(org.bukkit.Rotation.values()[Math.floorMod(rotation, 8)]);
        }
    }
    protected void addAdditionalSaveData(ValueOutput output) {

    }
    protected void readAdditionalSaveData(ValueInput input) {

    }
    public SoundEvent getRotateItemSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.item_frame.rotate_item");
    }
    public int getAnalogOutput() {
        return getItem().isEmpty() ? 0 : getRotation() % 8 + 1;
    }
    public Packet<ClientGamePacketListener> getAddEntityPacket(ServerEntity serverEntity) {
        return super.getAddEntityPacket(serverEntity);
    }
    public void recreateFromPacket(ClientboundAddEntityPacket packet) {
        super.recreateFromPacket(packet);
    }
    public ItemStack getPickResult() {
        ItemStack content = getItem();
        return content.isEmpty() ? getFrameItemStack() : content.copy();
    }
    protected ItemStack getFrameItemStack() {
        return new ItemStack(net.minecraft.world.item.Items.ITEM_FRAME);
    }
    public float getVisualRotationYInDegrees() {
        Direction direction = frame() == null ? Direction.NORTH : Direction.valueOf(frame().getFacing().name());
        return direction.getAxis().isHorizontal() ? 0.0F : (direction == Direction.DOWN ? 90.0F : -90.0F);
    }
    private static AABB boxFor(BlockPos blockPos, Direction direction, boolean framedMap) {
        Vec3 center = Vec3.atCenterOf(blockPos).relative(direction, -0.46875);
        double span = framedMap ? 1.0 : 0.75;
        Direction.Axis axis = direction.getAxis();
        return AABB.ofSize(center, axis == Direction.Axis.X ? 0.0625 : span, axis == Direction.Axis.Y ? 0.0625 : span,
            axis == Direction.Axis.Z ? 0.0625 : span);
    }
    public boolean hurtClient(DamageSource source) {
        return false;
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        if (isInvulnerableToBase(source) || isRemoved()) {
            return false;
        }
        dropItem(level, source.getEntity());
        discard();
        return true;
    }
    public InteractionResult interact(Player player, InteractionHand hand, Vec3 location) {
        ItemStack held = player.getItemInHand(hand);
        if (getItem().isEmpty()) {
            if (held.isEmpty()) {
                return InteractionResult.PASS;
            }
            setItem(held.copyWithCount(1));
            held.shrink(1);
        } else {
            setRotation(getRotation() + 1);
        }
        return InteractionResult.SUCCESS;
    }
    public void move(MoverType moverType, Vec3 delta) {
        return;
    }
}
