// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.synchronization;
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.tree.ArgumentCommandNode;
import com.mojang.brigadier.tree.CommandNode;
import com.mojang.brigadier.tree.LiteralCommandNode;
import com.mojang.brigadier.tree.RootCommandNode;
import com.mojang.serialization.JsonOps;
import it.unimi.dsi.fastutil.objects.ReferenceOpenHashSet;
import java.util.Collection;
import java.util.HashSet;
import java.util.Set;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.server.permissions.PermissionCheck;
import net.minecraft.server.permissions.PermissionProviderCheck;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
public class ArgumentUtils {
    private static final Logger kLog = LoggerFactory.getLogger(ArgumentUtils.class);
    private static final int kHasMin = 1;
    private static final int kHasMax = 2;
    public static int createNumberFlags(boolean hasMin, boolean hasMax) {
        return (hasMin ? kHasMin : 0) | (hasMax ? kHasMax : 0);
    }
    public static boolean numberHasMin(byte flags) {
        return (flags & kHasMin) != 0;
    }
    public static boolean numberHasMax(byte flags) {
        return (flags & kHasMax) != 0;
    }
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static <T extends ArgumentType<?>> void writeArgument(JsonObject out, T argument) {
        ArgumentTypeInfo.Template<T> template = ArgumentTypeInfos.unpack(argument);
        out.addProperty("type", "argument");
        out.addProperty("parser", String.valueOf(BuiltInRegistries.COMMAND_ARGUMENT_TYPE.getKey(template.type())));
        JsonObject properties = new JsonObject();
        ((ArgumentTypeInfo) template.type()).serializeToJson(template, properties);
        if (!properties.isEmpty()) {
            out.add("properties", properties);
        }
    }
    public static <S> JsonObject serializeNodeToJson(CommandDispatcher<S> dispatcher, CommandNode<S> node) {
        JsonObject out = new JsonObject();
        if (node instanceof RootCommandNode<S>) {
            out.addProperty("type", "root");
        } else if (node instanceof LiteralCommandNode<S>) {
            out.addProperty("type", "literal");
        } else if (node instanceof ArgumentCommandNode<S, ?> argument) {
            writeArgument(out, argument.getType());
        } else {
            kLog.error("Could not serialize node {} ({})!", node, node.getClass());
            out.addProperty("type", "unknown");
        }
        Collection<CommandNode<S>> children = node.getChildren();
        if (!children.isEmpty()) {
            JsonObject all = new JsonObject();
            for (CommandNode<S> child : children) {
                all.add(child.getName(), serializeNodeToJson(dispatcher, child));
            }
            out.add("children", all);
        }
        if (node.getCommand() != null) {
            out.addProperty("executable", true);
        }
        if (node.getRequirement() instanceof PermissionProviderCheck<?> check) {
            JsonElement permissions = PermissionCheck.CODEC.encodeStart(JsonOps.INSTANCE, check.test())
                    .getOrThrow(error -> new IllegalStateException("Failed to serialize requirement: " + error));
            out.add("permissions", permissions);
        }
        if (node.getRedirect() != null) {
            Collection<String> path = dispatcher.getPath(node.getRedirect());
            if (!path.isEmpty()) {
                JsonArray array = new JsonArray();
                path.forEach(array::add);
                out.add("redirect", array);
            }
        }
        return out;
    }
    public static <T> Set<ArgumentType<?>> findUsedArgumentTypes(CommandNode<T> node) {
        Set<ArgumentType<?>> found = new HashSet<>();
        collect(node, found, new ReferenceOpenHashSet<>());
        return found;
    }
    private static <T> void collect(CommandNode<T> node, Set<ArgumentType<?>> found, Set<CommandNode<T>> visited) {
        if (!visited.add(node)) {
            return;
        }
        if (node instanceof ArgumentCommandNode<T, ?> argument) {
            found.add(argument.getType());
        }
        node.getChildren().forEach(child -> collect(child, found, visited));
        if (node.getRedirect() != null) {
            collect(node.getRedirect(), found, visited);
        }
    }
}
