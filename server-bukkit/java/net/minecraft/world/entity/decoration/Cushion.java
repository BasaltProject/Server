// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.decoration;
import net.minecraft.core.BlockPos;
import net.minecraft.world.phys.shapes.BooleanOp;
import net.minecraft.world.phys.shapes.Shapes;
import net.minecraft.world.phys.shapes.VoxelShape;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.world.level.gamerules.GameRules;
import net.minecraft.world.item.Items;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LightningBolt;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.DyeColor;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
public class Cushion extends BlockAttachedEntity {
    private net.minecraft.world.item.DyeColor color = net.minecraft.world.item.DyeColor.WHITE;
    public Cushion(EntityType<Cushion> type, Level level) {
        super(type, level);
    }
    public DyeColor getColor() {
        return color;
    }
    public void setColor(DyeColor color) {
        this.color = color;
    }
    public void dropItem(ServerLevel level, Entity causedBy) {
        playSound(SoundEvents.CUSHION_BREAK, 1.0F, 1.0F);
        if (level.getGameRules().get(GameRules.ENTITY_DROPS) && !(causedBy instanceof Player player && player.hasInfiniteMaterials())) {
            spawnAtLocation(level, new ItemStack(Items.CUSHION.pick(color)));
        }
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public boolean hurtClient(DamageSource source) {
        return super.hurtClient(source);
    }
    public InteractionResult interact(Player player, InteractionHand hand, Vec3 location) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    protected void removePassenger(Entity passenger) {
        }
    public ItemStack getPickResult() {
        return new ItemStack(Items.CUSHION.pick(color));
    }
    protected void tickAtCheckInterval() {
        }
    public void destroyIfInFire(ServerLevel level) {
    }
    public void thunderHit(ServerLevel level, LightningBolt lightningBolt) {
        super.thunderHit(level, lightningBolt);
    }
    public void setPos(double x, double y, double z) {
        setPosRaw(x, y, z);
        super.setPos(x, y, z);
    }
    private static boolean anchorBelow(Level level, AABB box) {
        AABB slice = new AABB(box.minX, box.minY - 0.015625, box.minZ, Math.nextDown(box.maxX), box.minY, Math.nextDown(box.maxZ));
        for (BlockPos at : BlockPos.betweenClosed(slice.expandTowards(0.0, -0.125, 0.0))) {
            VoxelShape shape = level.getBlockState(at).getShape(level, at);
            if (!shape.isEmpty() && shape.bounds().move(at).intersects(slice)) {
                return true;
            }
        }
        return false;
    }
    private static boolean anchorBuried(Level level, AABB box) {
        AABB resting = new AABB(box.minX, box.minY, box.minZ, box.maxX, box.minY + 0.015625, box.maxZ).nextDeflated();
        VoxelShape exposed = Shapes.create(resting);
        for (VoxelShape collider : level.getBlockCollisions(null, resting)) {
            exposed = Shapes.join(exposed, collider, BooleanOp.ONLY_FIRST);
            if (exposed.isEmpty()) {
                return true;
            }
        }
        return false;
    }
    private static boolean coveredBySuffocating(Level level, AABB box) {
        for (BlockPos at : BlockPos.betweenClosed(box.nextDeflated())) {
            if (!level.getBlockState(at).isSuffocating(level, at)) {
                return false;
            }
        }
        return true;
    }
    public static boolean canBePlacedAt(Level level, AABB boundingBox) {
        return wouldSurviveAt(level, boundingBox) && !anchorBuried(level, boundingBox);
    }
    public static boolean wouldSurviveAt(Level level, AABB boundingBox) {
        return anchorBelow(level, boundingBox) && !coveredBySuffocating(level, boundingBox);
    }
    public boolean survives() {
        return wouldSurviveAt(level(), getBoundingBox());
    }
    protected void recalculateBoundingBox() {
        }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public <T> T get(DataComponentType<? extends T> type) {
        return super.get(type);
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        return false;
    }
}
