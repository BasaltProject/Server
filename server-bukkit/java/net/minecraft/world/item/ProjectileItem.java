// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import java.util.*;
import java.util.function.*;
import net.minecraft.core.*;
import net.minecraft.core.component.*;
import net.minecraft.network.chat.*;
import net.minecraft.server.level.*;
import net.minecraft.sounds.*;
import net.minecraft.world.*;
import net.minecraft.world.entity.*;
import net.minecraft.world.entity.player.*;
import net.minecraft.world.item.context.*;
import net.minecraft.world.level.*;
import net.minecraft.world.level.block.*;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.phys.*;
import net.minecraft.world.item.component.*;

import net.minecraft.world.entity.projectile.Projectile;
import net.minecraft.core.dispenser.BlockSource;
public interface ProjectileItem {
    Projectile asProjectile(Level level, Position position, ItemStack itemStack, Direction direction);

    default DispenseConfig createDispenseConfig() {
        return DispenseConfig.DEFAULT;
    }

    default void shoot(Projectile projectile, double xd, double yd, double zd, float pow, float uncertainty) {
        projectile.shoot(xd, yd, zd, pow, uncertainty);
    }

    public static record DispenseConfig(PositionFunction positionFunction, float uncertainty, float power, Optional<Integer> overrideDispenseEvent) {
        public static final DispenseConfig DEFAULT = builder().build();

        public static Builder builder() {
            return new Builder();
        }

        public static class Builder {
            private PositionFunction positionFunction = (source, direction) -> DispenserBlock.getDispensePosition(source, 0.7, new Vec3(0.0, 0.1, 0.0));
            private float uncertainty = 6.0F;
            private float power = 1.1F;
            private Optional<Integer> overrideDispenseEvent = Optional.empty();

            public Builder positionFunction(PositionFunction positionFunction) {
                this.positionFunction = positionFunction;
                return this;
            }

            public Builder uncertainty(float uncertainty) {
                this.uncertainty = uncertainty;
                return this;
            }

            public Builder power(float power) {
                this.power = power;
                return this;
            }

            public Builder overrideDispenseEvent(int dispenseEvent) {
                this.overrideDispenseEvent = Optional.of(dispenseEvent);
                return this;
            }

            public DispenseConfig build() {
                return new DispenseConfig(positionFunction, uncertainty, power, overrideDispenseEvent);
            }
        }
    }

    @FunctionalInterface
    public interface PositionFunction {
        Position getDispensePosition(BlockSource source, Direction direction);
    }
}
