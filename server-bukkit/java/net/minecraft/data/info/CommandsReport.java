// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.info;

import com.google.gson.JsonObject;
import com.mojang.brigadier.tree.CommandNode;
import java.util.concurrent.CompletableFuture;
import net.minecraft.commands.Commands;
import net.minecraft.core.HolderLookup;
import net.minecraft.data.CachedOutput;
import net.minecraft.data.DataProvider;
import net.minecraft.data.PackOutput;

public class CommandsReport implements DataProvider {
    private final PackOutput output;

    public CommandsReport(PackOutput output, CompletableFuture<HolderLookup.Provider> registries) {
        this.output = output;
    }

    private static <S> JsonObject describe(CommandNode<S> node) {
        JsonObject json = new JsonObject();
        json.addProperty("type", node.getRedirect() == null && node.getChildren().isEmpty() && node.getName().isEmpty() ? "root"
            : node.getUsageText().startsWith("<") ? "argument" : "literal");
        if (node.getCommand() != null) {
            json.addProperty("executable", true);
        }
        if (!node.getChildren().isEmpty()) {
            JsonObject children = new JsonObject();
            for (CommandNode<S> child : node.getChildren()) {
                children.add(child.getName(), describe(child));
            }
            json.add("children", children);
        }
        return json;
    }

    @Override
    public CompletableFuture<?> run(CachedOutput cache) {
        JsonObject root = describe(new Commands(Commands.CommandSelection.ALL, net.minecraft.commands.CommandBuildContext.simple(HolderLookup.Provider.create(java.util.stream.Stream.empty()), net.minecraft.world.flag.FeatureFlagSet.of())).getDispatcher().getRoot());
        root.addProperty("type", "root");
        return ReportJson.write(cache, output, "commands.json", root);
    }

    @Override
    public final String getName() {
        return "Command Syntax";
    }
}
