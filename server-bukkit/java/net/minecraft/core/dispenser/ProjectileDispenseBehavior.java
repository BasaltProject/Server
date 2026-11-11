// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.dispenser;
import net.minecraft.core.Direction;
import net.minecraft.core.Position;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.projectile.Projectile;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ProjectileItem;
import net.minecraft.world.level.block.DispenserBlock;
public class ProjectileDispenseBehavior extends DefaultDispenseItemBehavior {
    private final ProjectileItem projectileItem;
    private final ProjectileItem.DispenseConfig config;
    public ProjectileDispenseBehavior(Item item) {
        if (!(item instanceof ProjectileItem projectile)) {
            throw new IllegalArgumentException(item + " not instance of " + ProjectileItem.class.getSimpleName());
        }
        this.projectileItem = projectile;
        this.config = projectile.createDispenseConfig();
    }
    public ItemStack execute(BlockSource source, ItemStack dispensed) {
        ServerLevel level = source.level();
        Direction facing = source.state().getValue(DispenserBlock.FACING);
        Position origin = config.positionFunction().getDispensePosition(source, facing);
        Projectile shot = projectileItem.asProjectile(level, origin, dispensed, facing);
        Projectile.spawnProjectileUsingShoot(shot, level, dispensed, facing.getStepX(), facing.getStepY(), facing.getStepZ(), config.power(), config.uncertainty());
        dispensed.shrink(1);
        return dispensed;
    }
    protected void playSound(BlockSource source) {
        source.level().levelEvent(config.overrideDispenseEvent().orElse(1002), source.pos(), 0);
    }
}
