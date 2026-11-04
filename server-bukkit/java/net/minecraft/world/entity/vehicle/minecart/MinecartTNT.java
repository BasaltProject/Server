// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.vehicle.minecart;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Explosion;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class MinecartTNT extends AbstractMinecart {
    public MinecartTNT(EntityType<? extends MinecartTNT> type, Level level) {
        super(type, level);
    }
    private int fuse = -1;
    public void tick() {
        if (fuse > 0) {
            fuse--;
            if (fuse == 0 && level() instanceof ServerLevel server) {
                server.explode(this, getX(), getY(), getZ(), 4.0F, Level.ExplosionInteraction.TNT);
                discard();
            }
        }
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public void destroy(ServerLevel level, DamageSource source) {
        if (source.is(net.minecraft.tags.DamageTypeTags.IS_EXPLOSION)) {
            kill(level);
        } else {
            super.destroy(level, source);
        }
    }
    protected Item getDropItem() {
        return net.minecraft.world.item.Items.TNT_MINECART;
    }
    public ItemStack getPickResult() {
        return new ItemStack(net.minecraft.world.item.Items.TNT_MINECART);
    }
    protected void explode(DamageSource damageSource, double speedSqr) {
        }
    public void handleEntityEvent(byte id) {
    }
    public void primeFuse(DamageSource source) {
        if (fuse < 0) { fuse = 80; }
    }
    public int getFuse() {
        return fuse;
    }
    public boolean isPrimed() {
        return fuse > -1;
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected boolean shouldSourceDestroy(DamageSource source) {
        return false;
    }
}
