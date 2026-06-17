package org.basaltmc.bukkit.brigadier;

import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.ParseResults;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.suggestion.Suggestion;
import com.mojang.brigadier.tree.LiteralCommandNode;
import io.papermc.paper.command.brigadier.BasicCommand;
import io.papermc.paper.command.brigadier.CommandSourceStack;
import java.util.ArrayList;
import java.util.List;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.format.NamedTextColor;
import org.bukkit.Location;
import org.bukkit.command.Command;
import org.bukkit.command.CommandSender;
import org.bukkit.command.PluginIdentifiableCommand;
import org.bukkit.plugin.Plugin;

/** A command a plugin built with Brigadier or as a BasicCommand, run through the Bukkit command map. */
public final class BrigadierCommand extends Command implements PluginIdentifiableCommand {
    private final Plugin plugin;
    private final CommandDispatcher<CommandSourceStack> dispatcher;
    private final LiteralCommandNode<CommandSourceStack> node;
    private final BasicCommand basic;

    BrigadierCommand(String name, String description, List<String> aliases, Plugin plugin,
            CommandDispatcher<CommandSourceStack> dispatcher, LiteralCommandNode<CommandSourceStack> node,
            BasicCommand basic) {
        super(name, description == null ? "" : description, "/" + name, new ArrayList<>(aliases));
        this.plugin = plugin;
        this.dispatcher = dispatcher;
        this.node = node;
        this.basic = basic;
    }

    @Override
    public Plugin getPlugin() {
        return plugin;
    }

    private static String input(String label, String[] args) {
        return args.length == 0 ? label : label + " " + String.join(" ", args);
    }

    @Override
    public boolean execute(CommandSender sender, String label, String[] args) {
        BasaltCommandSourceStack source = BasaltCommandSourceStack.of(sender);
        if (basic != null) {
            if (!basic.canUse(sender)) {
                sender.sendMessage(Component.text("I'm sorry, but you do not have permission to perform this command.",
                    NamedTextColor.RED));
                return true;
            }
            try {
                basic.execute(source, args);
            } catch (CommandSyntaxException e) {
                sender.sendMessage(Component.text(e.getMessage(), NamedTextColor.RED));
                return false;
            }
            return true;
        }
        try {
            dispatcher.execute(input(label, args), source);
        } catch (CommandSyntaxException e) {
            sender.sendMessage(Component.text(e.getMessage(), NamedTextColor.RED));
            return false;
        }
        return true;
    }

    @Override
    public List<String> tabComplete(CommandSender sender, String alias, String[] args, Location location) {
        BasaltCommandSourceStack source = BasaltCommandSourceStack.of(sender);
        if (basic != null) {
            return basic.canUse(sender) ? new ArrayList<>(basic.suggest(source, args)) : List.of();
        }
        ParseResults<CommandSourceStack> parse = dispatcher.parse(input(alias, args), source);
        try {
            List<String> out = new ArrayList<>();
            for (Suggestion suggestion : dispatcher.getCompletionSuggestions(parse).get().getList()) {
                out.add(suggestion.getText());
            }
            return out;
        } catch (java.util.concurrent.ExecutionException | InterruptedException e) {
            return List.of();
        }
    }

    @Override
    public boolean testPermissionSilent(CommandSender target) {
        if (basic != null) {
            return basic.canUse(target);
        }
        return node.getRequirement().test(BasaltCommandSourceStack.of(target));
    }
}
