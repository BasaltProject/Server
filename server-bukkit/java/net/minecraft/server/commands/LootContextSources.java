// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import net.minecraft.world.level.storage.loot.parameters.LootContextParamSets;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import com.mojang.brigadier.builder.ArgumentBuilder;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import java.util.function.Consumer;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.LootParams;
public class LootContextSources {
    public static <T extends ArgumentBuilder<CommandSourceStack, T>> T addContextSources(T node, NodeVisitor factory) {
        return node.then(withContext(net.minecraft.commands.Commands.literal("default"), factory, (context, params) -> params.create(LootContextParamSets.COMMAND_COMPUTE_DEFAULT)))
            .then(net.minecraft.commands.Commands.literal("block").then(withContext(net.minecraft.commands.Commands.argument("computePos",
                net.minecraft.commands.arguments.coordinates.BlockPosArgument.blockPos()), factory, (context, params) -> {
                net.minecraft.core.BlockPos pos = net.minecraft.commands.arguments.coordinates.BlockPosArgument.getLoadedBlockPos(context, "computePos");
                net.minecraft.server.level.ServerLevel level = context.getSource().getLevel();
                return params.withParameter(LootContextParams.BLOCK_STATE, level.getBlockState(pos))
                    .withOptionalParameter(LootContextParams.BLOCK_ENTITY, level.getBlockEntity(pos)).create(LootContextParamSets.COMMAND_COMPUTE_POSITION);
            })))
            .then(net.minecraft.commands.Commands.literal("entity").then(withContext(net.minecraft.commands.Commands.argument("computeTarget",
                net.minecraft.commands.arguments.EntityArgument.entity()), factory, (context, params) ->
                params.withParameter(LootContextParams.TARGET_ENTITY, net.minecraft.commands.arguments.EntityArgument.getEntity(context, "computeTarget"))
                    .create(LootContextParamSets.COMMAND_COMPUTE_ENTITY))));
    }
    private static <B extends ArgumentBuilder<CommandSourceStack, ?>> B withContext(B node, NodeVisitor visitor, ContextDecorator decorator) {
        visitor.visit(decorator, node::then);
        return node;
    }

    public interface ContextDecorator {
        LootParams customize(CommandContext<CommandSourceStack> context, LootParams.Builder params) throws CommandSyntaxException;
        default LootParams createParams(CommandContext<CommandSourceStack> context) throws CommandSyntaxException {
            CommandSourceStack source = context.getSource();
            return customize(context, new LootParams.Builder(source.getLevel()).withOptionalParameter(LootContextParams.THIS_ENTITY, source.getEntity())
                .withParameter(LootContextParams.ORIGIN, source.getPosition()));
        }
        default LootContext createContext(CommandContext<CommandSourceStack> context) throws CommandSyntaxException {
            return new LootContext.Builder(createParams(context)).create(java.util.Optional.empty());
        }
    }
    public interface NodeVisitor {
        void visit(ContextDecorator contextDecorator, Consumer<ArgumentBuilder<CommandSourceStack, ?>> output);
    }
}
