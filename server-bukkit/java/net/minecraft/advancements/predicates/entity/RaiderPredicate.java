// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.*;
import net.minecraft.advancements.predicates.*;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.*;
import net.minecraft.world.phys.Vec3;
public record RaiderPredicate(boolean hasRaid, boolean isCaptain) implements EntitySubPredicate {
    public static final Codec<RaiderPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        Codec.BOOL.optionalFieldOf("has_raid", false).forGetter(RaiderPredicate::hasRaid),
        Codec.BOOL.optionalFieldOf("is_captain", false).forGetter(RaiderPredicate::isCaptain)).apply(i, RaiderPredicate::new));
    public static final RaiderPredicate CAPTAIN_WITHOUT_RAID = new RaiderPredicate(false, true);
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        return entity instanceof net.minecraft.world.entity.raid.Raider raider && raider.hasRaid() == hasRaid && raider.isCaptain() == isCaptain;
    }
}
