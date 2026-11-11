// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity.trialspawner;

import java.util.List;
import java.util.Objects;
import java.util.UUID;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.ClipContext;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.entity.EntityTypeTest;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.HitResult;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.phys.shapes.CollisionContext;

public interface PlayerDetector {
    PlayerDetector NO_CREATIVE_PLAYERS = players(false);
    PlayerDetector INCLUDING_CREATIVE_PLAYERS = players(true);
    PlayerDetector SHEEP = (level, selector, pos, range, sight) -> {
        AABB area = new AABB(pos).inflate(range);
        return selector.getEntities(level, EntityTypes.SHEEP, area, LivingEntity::isAlive).stream()
            .filter(sheep -> visible(level, pos, sheep, sight)).map(Entity::getUUID).toList();
    };

    List<UUID> detect(ServerLevel level, EntitySelector selector, BlockPos spawnerPos, double requiredPlayerRange, boolean requireLineOfSight);

    private static PlayerDetector players(boolean includeCreative) {
        return (level, selector, pos, range, sight) -> selector
            .getPlayers(level, p -> p.blockPosition().closerThan(pos, range) && (includeCreative || !p.isCreative()) && !p.isSpectator())
            .stream().filter(p -> visible(level, pos, p, sight)).map(Entity::getUUID).toList();
    }

    private static boolean visible(Level level, BlockPos pos, Entity target, boolean needed) {
        return !needed || inLineOfSight(level, Vec3.atCenterOf(pos), target.getEyePosition());
    }

    private static boolean inLineOfSight(Level level, Vec3 origin, Vec3 dest) {
        BlockHitResult hit = level.clip(new ClipContext(dest, origin, ClipContext.Block.VISUAL, ClipContext.Fluid.NONE, CollisionContext.empty()));
        return hit.getBlockPos().equals(BlockPos.containing(origin)) || hit.getType() == HitResult.Type.MISS;
    }

    public interface EntitySelector {
        EntitySelector SELECT_FROM_LEVEL = new EntitySelector() {
            public List<ServerPlayer> getPlayers(ServerLevel level, Predicate<? super Player> selector) {
                return level.getPlayers(selector);
            }

            public <T extends Entity> List<T> getEntities(ServerLevel level, EntityTypeTest<Entity, T> type, AABB bb, Predicate<? super T> selector) {
                return level.getEntities(type, bb, selector);
            }
        };

        List<? extends Player> getPlayers(ServerLevel level, Predicate<? super Player> selector);

        <T extends Entity> List<T> getEntities(ServerLevel level, EntityTypeTest<Entity, T> type, AABB bb, Predicate<? super T> selector);

        static EntitySelector onlySelectPlayer(Player player) {
            return onlySelectPlayers(List.of(player));
        }

        static EntitySelector onlySelectPlayers(List<Player> only) {
            return new EntitySelector() {
                public List<Player> getPlayers(ServerLevel level, Predicate<? super Player> selector) {
                    return only.stream().filter(selector).toList();
                }

                public <T extends Entity> List<T> getEntities(ServerLevel level, EntityTypeTest<Entity, T> type, AABB bb, Predicate<? super T> selector) {
                    Stream<T> cast = only.stream().map(type::tryCast);
                    return cast.filter(Objects::nonNull).filter(selector).toList();
                }
            };
        }
    }
}
