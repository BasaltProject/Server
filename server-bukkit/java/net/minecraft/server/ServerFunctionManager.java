// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;

import com.mojang.brigadier.CommandDispatcher;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.functions.CommandFunction;
import net.minecraft.resources.Identifier;
import org.basaltmc.bukkit.LeftoverNatives;

/** Function names come from the native function library; running one goes through the function command. */
public class ServerFunctionManager {
    public ServerFunctionManager(MinecraftServer server, ServerFunctionLibrary library) {
    }

    public CommandDispatcher<CommandSourceStack> getDispatcher() {
        return null;
    }

    public void tick() {
    }

    public void execute(CommandFunction<CommandSourceStack> functionIn, CommandSourceStack sender) {
        org.bukkit.Bukkit.dispatchCommand(org.bukkit.Bukkit.getConsoleSender(), "function " + functionIn.id());
    }

    public void replaceLibrary(ServerFunctionLibrary library) {
    }

    public CommandSourceStack getGameLoopSender() {
        return null;
    }

    public Optional<CommandFunction<CommandSourceStack>> get(Identifier id) {
        return Optional.empty();
    }

    public List<CommandFunction<CommandSourceStack>> getTag(Identifier id) {
        return List.of();
    }

    public Iterable<Identifier> getFunctionNames() {
        return identifiers(LeftoverNatives.functionIds());
    }

    public Iterable<Identifier> getTagNames() {
        return identifiers(LeftoverNatives.functionTagIds());
    }

    private static List<Identifier> identifiers(String[] names) {
        List<Identifier> out = new ArrayList<>();
        for (String name : names) {
            out.add(Identifier.parse(name));
        }
        return out;
    }
}
