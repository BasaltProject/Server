// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates.entity;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.*;
import net.minecraft.advancements.predicates.*;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.*;
import net.minecraft.world.phys.Vec3;
public record TeamPredicate(String team) implements EntitySubPredicate {
    public static final Codec<TeamPredicate> CODEC = Codec.STRING.xmap(TeamPredicate::new, TeamPredicate::team);
    @Override
    public boolean matches(Entity entity, ServerLevel level, Vec3 position) {
        net.minecraft.world.scores.Team found = entity.getTeam();
        return found != null && team.equals(found.getName());
    }
}
