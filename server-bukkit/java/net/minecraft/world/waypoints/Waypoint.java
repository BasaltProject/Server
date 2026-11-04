// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.waypoints;

import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.Optional;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.attributes.AttributeModifier;
import net.minecraft.world.item.Item;
public interface Waypoint {
    AttributeModifier WAYPOINT_TRANSMIT_RANGE_HIDE_MODIFIER = null;
    static Item.Properties addHideAttribute(Item.Properties properties) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.waypoints.Waypoint#addHideAttribute");
    }
    public static class Icon {
        public static final Codec<Icon> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.waypoints.Waypoint.Icon.class, "CODEC");
        public static final StreamCodec<ByteBuf, Icon> STREAM_CODEC = StreamCodec.composite(
            ResourceKey.streamCodec(WaypointStyleAssets.ROOT_ID), icon -> icon.style,
            ByteBufCodecs.optional(ByteBufCodecs.RGB_COLOR), icon -> icon.color,
            Icon::new);
        public static final Icon NULL = new Icon();
        public ResourceKey<WaypointStyleAsset> style = WaypointStyleAssets.DEFAULT;
        public Optional<Integer> color = Optional.empty();

        public Icon() {
        }

        private Icon(ResourceKey<WaypointStyleAsset> style, Optional<Integer> color) {
            this.style = style;
            this.color = color;
        }

        public boolean hasData() {
            return style != WaypointStyleAssets.DEFAULT || color.isPresent();
        }

        public Icon cloneAndAssignStyle(LivingEntity livingEntity) {
            Optional<Integer> chosen = color.or(() -> Optional.ofNullable(livingEntity.getTeam()).flatMap(team -> team.getColor()).map(teamColor -> teamColor == net.minecraft.world.scores.TeamColor.BLACK ? -13619152 : teamColor.rgb()));
            return chosen.isEmpty() ? this : new Icon(style, chosen);
        }

        public void copyFrom(Icon other) {
            this.color = other.color;
            this.style = other.style;
        }
    }
}
