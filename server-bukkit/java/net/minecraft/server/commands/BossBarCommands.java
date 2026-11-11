// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import net.minecraft.network.chat.Component;
import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.suggestion.SuggestionProvider;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.server.bossevents.CustomBossEvent;
public class BossBarCommands {
    public static final SuggestionProvider<CommandSourceStack> SUGGEST_BOSS_BAR = (context, builder) -> net.minecraft.commands.SharedSuggestionProvider.suggestResource(
        context.getSource().getServer().getCustomBossEvents().getIds(), builder);
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher, CommandBuildContext context) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "bossbar");
    }
    public static CustomBossEvent getBossBar(CommandContext<CommandSourceStack> context) throws CommandSyntaxException {
        net.minecraft.resources.Identifier id = net.minecraft.commands.arguments.IdentifierArgument.getId(context, "id");
        CustomBossEvent event = context.getSource().getServer().getCustomBossEvents().get(id);
        if (event == null) {
            throw new com.mojang.brigadier.exceptions.DynamicCommandExceptionType(name -> Component.translatable("commands.bossbar.unknown", name)).create(id.toString());
        }
        return event;
    }
}
