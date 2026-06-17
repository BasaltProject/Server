package org.basaltmc.bukkit.brigadier;

import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.builder.LiteralArgumentBuilder;
import com.mojang.brigadier.tree.LiteralCommandNode;
import io.papermc.paper.command.brigadier.BasicCommand;
import io.papermc.paper.command.brigadier.CommandRegistrationFlag;
import io.papermc.paper.command.brigadier.CommandSourceStack;
import io.papermc.paper.command.brigadier.Commands;
import io.papermc.paper.plugin.configuration.PluginMeta;
import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import org.bukkit.Bukkit;
import org.bukkit.plugin.Plugin;

/** Where a plugin registers the commands it builds with Brigadier: they land in the Bukkit command map. */
public final class BasaltCommands implements Commands {
    private static final CommandDispatcher<CommandSourceStack> kDispatcher = new CommandDispatcher<>();

    private final Plugin owner;

    public BasaltCommands(Plugin owner) {
        this.owner = owner;
    }

    @Override
    public CommandDispatcher<CommandSourceStack> getDispatcher() {
        return kDispatcher;
    }

    private Plugin ownerOf(PluginMeta meta) {
        Plugin found = meta == null ? null : Bukkit.getPluginManager().getPlugin(meta.getName());
        return found != null ? found : owner;
    }

    private Set<String> add(Plugin plugin, String name, String description, Collection<String> aliases,
            LiteralCommandNode<CommandSourceStack> node, BasicCommand basic) {
        String label = name.toLowerCase(Locale.ROOT);
        List<String> extra = new ArrayList<>();
        for (String alias : aliases) {
            extra.add(alias.toLowerCase(Locale.ROOT));
        }
        if (node != null) {
            kDispatcher.getRoot().getChildren().removeIf(child -> child.getName().equals(node.getName()));
            kDispatcher.getRoot().addChild(node);
            for (String alias : extra) {
                kDispatcher.register(LiteralArgumentBuilder.<CommandSourceStack>literal(alias)
                    .requires(node.getRequirement()).redirect(node));
            }
        }
        BrigadierCommand command = new BrigadierCommand(label, description, extra, plugin, kDispatcher, node, basic);
        Bukkit.getCommandMap().register(plugin.getName().toLowerCase(Locale.ROOT), command);
        Set<String> registered = new LinkedHashSet<>();
        registered.add(label);
        registered.addAll(extra);
        return Set.copyOf(registered);
    }

    @Override
    public Set<String> register(LiteralCommandNode<CommandSourceStack> node, String description,
            Collection<String> aliases) {
        return add(owner, node.getLiteral(), description, aliases, node, null);
    }

    @Override
    public Set<String> register(PluginMeta pluginMeta, LiteralCommandNode<CommandSourceStack> node,
            String description, Collection<String> aliases) {
        return add(ownerOf(pluginMeta), node.getLiteral(), description, aliases, node, null);
    }

    @Override
    public Set<String> registerWithFlags(PluginMeta pluginMeta, LiteralCommandNode<CommandSourceStack> node,
            String description, Collection<String> aliases, Set<CommandRegistrationFlag> flags) {
        return register(pluginMeta, node, description, aliases);
    }

    @Override
    public Set<String> register(String label, String description, Collection<String> aliases, BasicCommand command) {
        return add(owner, label, description, aliases, null, command);
    }

    @Override
    public Set<String> register(PluginMeta pluginMeta, String label, String description, Collection<String> aliases,
            BasicCommand command) {
        return add(ownerOf(pluginMeta), label, description, aliases, null, command);
    }
}
