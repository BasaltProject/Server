// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands.item;
import net.minecraft.network.chat.Component;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.Dynamic3CommandExceptionType;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.core.BlockPos;
import net.minecraft.server.commands.ArgProvider;
import net.minecraft.server.commands.CommandResponseTracker;
import net.minecraft.world.Container;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.slot.SlotCollection;
import net.minecraft.world.item.slot.SlotSource;
public record BlockItemAccessor(BlockPos pos) implements ItemAccessor<BlockPos> {
    public static final ArgProvider.Factory<ItemAccessor<?>> PROVIDER = arg -> ArgProvider.create("block",
        () -> net.minecraft.commands.Commands.argument(arg, net.minecraft.commands.arguments.coordinates.BlockPosArgument.blockPos()),
        context -> new BlockItemAccessor(net.minecraft.commands.arguments.coordinates.BlockPosArgument.getLoadedBlockPos(context, arg)));
    private static final CommandResponseTracker.Messages<BlockPos> RESPONSE_SET = CommandResponseTracker.messages(ItemCommands.ERROR_TARGET_NO_CHANGES,
        (pos, slotCount) -> Component.translatable("commands.item.block.replace.success", slotCount, pos.getX(), pos.getY(), pos.getZ()),
        (count, total) -> Component.empty());
    private static final CommandResponseTracker.MessagesWithArg<BlockPos, ItemStack> RESPONSE_SET_KNOWN_ITEM = CommandResponseTracker.messages(
        ItemCommands.ERROR_TARGET_NO_CHANGES_KNOWN_ITEM::create,
        (pos, slotCount, item) -> Component.translatable("commands.item.block.replace.success.known_item", slotCount, pos.getX(), pos.getY(), pos.getZ(), item.getDisplayName()),
        (count, total, item) -> Component.empty());
    private static final CommandResponseTracker.Messages<BlockPos> RESPONSE_MODIFY = CommandResponseTracker.messages(ItemCommands.ERROR_TARGET_NO_CHANGES,
        (pos, slotCount) -> Component.translatable("commands.item.block.modify.success", slotCount, pos.getX(), pos.getY(), pos.getZ()),
        (count, total) -> Component.empty());

    private SlotCollection blockSlots(CommandSourceStack source, SlotSource slotSource, Dynamic3CommandExceptionType exceptionType) throws CommandSyntaxException {
        return ItemCommands.getSlotsFromProvider(source, getContainer(source, pos, exceptionType), slotSource);
    }
    public SlotCollection getSlots(CommandSourceStack source, SlotSource slotSource) throws CommandSyntaxException {
        return blockSlots(source, slotSource, ItemCommands.ERROR_SOURCE_NOT_A_CONTAINER);
    }
    public void setItems(CommandSourceStack source, SlotSource slotSource, ItemAccessor.SetterFunction<BlockPos> function) throws CommandSyntaxException {
        function.apply(pos, blockSlots(source, slotSource, ItemCommands.ERROR_TARGET_NOT_A_CONTAINER));
    }
    public int getReplaceSuccess(CommandSourceStack source, CommandResponseTracker<BlockPos> tracker, ItemStack knownItem) throws CommandSyntaxException {
        return knownItem != null ? tracker.sendFeedback(source, true, RESPONSE_SET_KNOWN_ITEM, knownItem) : tracker.sendFeedback(source, true, RESPONSE_SET);
    }
    public int getModifySuccess(CommandSourceStack source, CommandResponseTracker<BlockPos> tracker) throws CommandSyntaxException {
        return tracker.sendFeedback(source, true, RESPONSE_MODIFY);
    }
    public static Container getContainer(CommandSourceStack source, BlockPos pos, Dynamic3CommandExceptionType exceptionType) throws CommandSyntaxException {
        if (source.getLevel().getBlockEntity(pos) instanceof Container container) {
            return container;
        }
        throw exceptionType.create(pos.getX(), pos.getY(), pos.getZ());
    }
}
