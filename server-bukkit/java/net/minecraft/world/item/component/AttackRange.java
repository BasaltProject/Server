// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.function.Predicate;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.HitResult;
import net.minecraft.world.phys.Vec3;
public record AttackRange(float minReach, float maxReach, float minCreativeReach, float maxCreativeReach, float hitboxMargin, float mobFactor) {
    public static final Codec<AttackRange> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.AttackRange.class, "CODEC");
    public static final StreamCodec<ByteBuf, AttackRange> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.AttackRange.class, "STREAM_CODEC");
    public static AttackRange defaultFor(LivingEntity livingEntity) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.AttackRange#defaultFor");
    }
    public HitResult getClosesetHit(Entity attacker, float partial, Predicate<Entity> matching) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.AttackRange#getClosesetHit");
    }
    public float effectiveMinRange(Entity entity) {
        if (entity instanceof net.minecraft.world.entity.player.Player player) {
            return player.isCreative() ? minCreativeReach : minReach;
        }
        return minReach * mobFactor;
    }
    public float effectiveMaxRange(Entity entity) {
        if (entity instanceof net.minecraft.world.entity.player.Player player) {
            return player.isCreative() ? maxCreativeReach : maxReach;
        }
        return maxReach * mobFactor;
    }
    public boolean isInRange(LivingEntity attacker, Vec3 location) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.AttackRange#isInRange");
    }
    public boolean isInRange(LivingEntity attacker, AABB boundingBox, double extraBuffer) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.AttackRange#isInRange");
    }
}
