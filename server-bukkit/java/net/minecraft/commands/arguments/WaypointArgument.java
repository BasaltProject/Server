// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import net.minecraft.network.chat.Component;
import net.minecraft.world.entity.Entity;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.world.waypoints.WaypointTransmitter;
public class WaypointArgument {
    public static final SimpleCommandExceptionType ERROR_NOT_A_WAYPOINT = new SimpleCommandExceptionType(Component.translatable("commands.waypoint.not_a_waypoint"));
    public static WaypointTransmitter getWaypoint(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        Entity entity = EntityArgument.getEntity(context, name);
        if (entity instanceof WaypointTransmitter transmitter) {
            return transmitter;
        }
        throw ERROR_NOT_A_WAYPOINT.create();
    }
}
