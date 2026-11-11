// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import net.minecraft.network.chat.Component;
import com.mojang.brigadier.CommandDispatcher;
import com.mojang.datafixers.util.Pair;
import java.time.Duration;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.arguments.ResourceOrTagArgument;
import net.minecraft.commands.arguments.ResourceOrTagKeyArgument;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
public class LocateCommand {
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher, CommandBuildContext context) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "locate");
    }
    private static int report(CommandSourceStack source, BlockPos from, BlockPos found, String key, boolean includeY, String label) {
        int dx = found.getX() - from.getX();
        int dz = found.getZ() - from.getZ();
        int distance = includeY ? net.minecraft.util.Mth.floor(net.minecraft.util.Mth.sqrt((float) from.distSqr(found)))
            : net.minecraft.util.Mth.floor(net.minecraft.util.Mth.sqrt((float) (dx * dx + dz * dz)));
        String shownY = includeY ? String.valueOf(found.getY()) : "~";
        Component coordinates = net.minecraft.network.chat.ComponentUtils.wrapInSquareBrackets(
                Component.translatable("chat.coordinates", found.getX(), shownY, found.getZ()))
            .withStyle(style -> style.withColor(net.minecraft.ChatFormatting.GREEN)
                .withClickEvent(new net.minecraft.network.chat.ClickEvent.SuggestCommand("/tp @s " + found.getX() + " " + shownY + " " + found.getZ()))
                .withHoverEvent(new net.minecraft.network.chat.HoverEvent.ShowText(Component.translatable("chat.coordinates.tooltip"))));
        source.sendSuccess(() -> Component.translatable(key, label, coordinates, distance), false);
        return distance;
    }

    public static int showLocateResult(CommandSourceStack source, ResourceOrTagArgument.Result<?> name, BlockPos sourcePos, Pair<BlockPos, ? extends Holder<?>> found, String successMessageKey, boolean includeY, Duration taskDuration) {
        String label = name.unwrap().map(element -> name.asPrintable(), tag -> name.asPrintable() + " (" + found.getSecond().getRegisteredName() + ")");
        return report(source, sourcePos, found.getFirst(), successMessageKey, includeY, label);
    }
    public static int showLocateResult(CommandSourceStack source, ResourceOrTagKeyArgument.Result<?> name, BlockPos sourcePos, Pair<BlockPos, ? extends Holder<?>> found, String successMessageKey, boolean includeY, Duration taskDuration) {
        String label = name.unwrap().map(element -> element.identifier().toString(), tag -> "#" + tag.location() + " (" + found.getSecond().getRegisteredName() + ")");
        return report(source, sourcePos, found.getFirst(), successMessageKey, includeY, label);
    }
}
