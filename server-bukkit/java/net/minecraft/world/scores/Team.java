// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.scores;

import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.Collection;
import java.util.Optional;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.MutableComponent;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.StringRepresentable;
public abstract class Team {
    public enum Visibility implements StringRepresentable { ALWAYS("always", 0), NEVER("never", 1), HIDE_FOR_OTHER_TEAMS("hideForOtherTeams", 2),
        HIDE_FOR_OWN_TEAM("hideForOwnTeam", 3);
        public static final Codec<Visibility> CODEC = StringRepresentable.fromEnum(Visibility::values);
        public static final StreamCodec<ByteBuf, Visibility> STREAM_CODEC = net.minecraft.network.codec.ByteBufCodecs.idMapper(i -> values()[i], v -> v.id);
        public final String name;
        public final int id;
        private Visibility(String name, int id) {
            this.name = name;
            this.id = id;
        }
        public Component getDisplayName() {
            return Component.translatable("team.visibility." + name);
        }
        public String getSerializedName() {
            return name;
        }
}
    public enum CollisionRule implements StringRepresentable { ALWAYS("always", 0), NEVER("never", 1), PUSH_OTHER_TEAMS("pushOtherTeams", 2),
        PUSH_OWN_TEAM("pushOwnTeam", 3);
        public static final Codec<CollisionRule> CODEC = StringRepresentable.fromEnum(CollisionRule::values);
        public static final StreamCodec<ByteBuf, CollisionRule> STREAM_CODEC = net.minecraft.network.codec.ByteBufCodecs.idMapper(i -> values()[i], v -> v.id);
        public final String name;
        public final int id;
        private CollisionRule(String name, int id) {
            this.name = name;
            this.id = id;
        }
        public Component getDisplayName() {
            return Component.translatable("team.collision." + name);
        }
        public String getSerializedName() {
            return name;
        }
}
    public abstract String getName();
    public abstract java.util.Collection<String> getPlayers();
    public boolean isAlliedTo(Team other) {
        return other != null && this == other;
    }
    public MutableComponent getFormattedName(Component teamMemberName) {
        return PlayerTeam.formatNameForTeam(this, teamMemberName);
    }
    public boolean canSeeFriendlyInvisibles() {
        return true;
    }
    public boolean isAllowFriendlyFire() {
        return true;
    }
    public Visibility getNameTagVisibility() {
        return Visibility.ALWAYS;
    }
    public Optional<TeamColor> getColor() {
        return Optional.empty();
    }
    public Visibility getDeathMessageVisibility() {
        return Visibility.ALWAYS;
    }
    public CollisionRule getCollisionRule() {
        return CollisionRule.ALWAYS;
    }
}
