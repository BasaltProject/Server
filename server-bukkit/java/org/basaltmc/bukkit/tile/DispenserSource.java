package org.basaltmc.bukkit.tile;

import org.basaltmc.bukkit.BasaltWorld;
import org.basaltmc.bukkit.entity.Cored;
import org.basaltmc.bukkit.entity.EntitySpawner;
import org.bukkit.Location;
import org.bukkit.block.Block;
import org.bukkit.block.BlockFace;
import org.bukkit.entity.EntityType;
import org.bukkit.entity.Projectile;
import org.bukkit.event.entity.CreatureSpawnEvent.SpawnReason;
import org.bukkit.projectiles.BlockProjectileSource;
import org.bukkit.util.Vector;

final class DispenserSource implements BlockProjectileSource {
    private static final double kPower = 1.1;
    private static final double kMouth = 0.7;
    private final Tile tile;

    DispenserSource(Tile tile) {
        this.tile = tile;
    }

    @Override
    public Block getBlock() {
        return tile.live();
    }

    @Override
    public <T extends Projectile> T launchProjectile(Class<? extends T> projectile) {
        return launchProjectile(projectile, null);
    }

    @Override
    public <T extends Projectile> T launchProjectile(Class<? extends T> projectile, Vector velocity) {
        return launchProjectile(projectile, velocity, null);
    }

    @Override
    @SuppressWarnings("unchecked")
    public <T extends Projectile> T launchProjectile(Class<? extends T> projectile, Vector velocity,
            java.util.function.Consumer<? super T> before) {
        String facing = tile.property("facing");
        BlockFace face = facing == null ? BlockFace.NORTH
            : BlockFace.valueOf(facing.toUpperCase(java.util.Locale.ROOT));
        Vector direction = face.getDirection();
        EntityType kind = EntitySpawner.typeOf(projectile);
        Location at = new Location(tile.world, tile.x + 0.5 + direction.getX() * kMouth,
            tile.y + 0.5 + direction.getY() * kMouth - (face.getModY() == 0 ? 0.125 : 0.0),
            tile.z + 0.5 + direction.getZ() * kMouth);
        Vector launch = velocity != null ? velocity : direction.clone().multiply(kPower);
        return (T) EntitySpawner.spawn((BasaltWorld) tile.world, at, kind, SpawnReason.DISPENSE_EGG, made -> {
            if (made instanceof Cored cored) {
                cored.core().velocity(launch);
            }
            if (before != null) {
                before.accept((T) made);
            }
        });
    }
}
