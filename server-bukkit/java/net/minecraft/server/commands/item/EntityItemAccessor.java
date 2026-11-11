// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands.item;
import net.minecraft.network.chat.Component;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import java.util.Collection;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.server.commands.ArgProvider;
import net.minecraft.server.commands.CommandResponseTracker;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.slot.SlotCollection;
import net.minecraft.world.item.slot.SlotSource;
public record EntityItemAccessor(Collection<? extends Entity> entities) implements ItemAccessor<Entity> {
    public static final ArgProvider.Factory<ItemAccessor<?>> PROVIDER = arg -> ArgProvider.create("entity",
        () -> net.minecraft.commands.Commands.argument(arg, net.minecraft.commands.arguments.EntityArgument.entities()),
        context -> new EntityItemAccessor(net.minecraft.commands.arguments.EntityArgument.getEntities(context, arg)));
    private static final CommandResponseTracker.Messages<Entity> RESPONSE_SET = CommandResponseTracker.messages(ItemCommands.ERROR_TARGET_NO_CHANGES,
        (entity, slotCount) -> Component.translatable("commands.item.entity.replace.success.single", slotCount, entity.getDisplayName()),
        (entityCount, total) -> Component.translatable("commands.item.entity.replace.success.multiple", entityCount));
    private static final CommandResponseTracker.MessagesWithArg<Entity, ItemStack> RESPONSE_SET_KNOWN_ITEM = CommandResponseTracker.messages(
        ItemCommands.ERROR_TARGET_NO_CHANGES_KNOWN_ITEM::create,
        (entity, slotCount, item) -> Component.translatable("commands.item.entity.replace.success.single.known_item", slotCount, entity.getDisplayName(), item.getDisplayName()),
        (entityCount, total, item) -> Component.translatable("commands.item.entity.replace.success.multiple.known_item", entityCount, item.getDisplayName()));
    private static final CommandResponseTracker.Messages<Entity> RESPONSE_MODIFY = CommandResponseTracker.messages(ItemCommands.ERROR_TARGET_NO_CHANGES,
        (entity, slotCount) -> Component.translatable("commands.item.entity.modify.success.single", slotCount, entity.getDisplayName()),
        (entityCount, total) -> Component.translatable("commands.item.entity.modify.success.multiple", entityCount));
    public SlotCollection getSlots(CommandSourceStack source, SlotSource slotSource) {
        java.util.List<SlotCollection> parts = new java.util.ArrayList<>();
        for (Entity entity : entities) {
            parts.add(ItemCommands.getSlotsFromProvider(source, entity, slotSource));
        }
        return SlotCollection.concat(parts);
    }
    public void setItems(CommandSourceStack source, SlotSource slotSource, ItemAccessor.SetterFunction<Entity> function) {
        for (Entity entity : entities) {
            int updated = function.apply(entity, ItemCommands.getSlotsFromProvider(source, entity, slotSource));
            if (updated > 0 && entity instanceof net.minecraft.server.level.ServerPlayer player) {
                player.containerMenu.broadcastChanges();
            }
        }
    }
    public int getReplaceSuccess(CommandSourceStack source, CommandResponseTracker<Entity> tracker, ItemStack knownItem) throws CommandSyntaxException {
        return knownItem != null ? tracker.sendFeedback(source, true, RESPONSE_SET_KNOWN_ITEM, knownItem) : tracker.sendFeedback(source, true, RESPONSE_SET);
    }
    public int getModifySuccess(CommandSourceStack source, CommandResponseTracker<Entity> tracker) throws CommandSyntaxException {
        return tracker.sendFeedback(source, true, RESPONSE_MODIFY);
    }
}
