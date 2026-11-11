// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands.item;
import net.minecraft.network.chat.Component;
import net.minecraft.world.level.storage.loot.LootParams;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.level.storage.loot.parameters.LootContextParamSets;
import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.exceptions.Dynamic3CommandExceptionType;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import java.util.Collection;
import java.util.List;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.server.commands.ArgProvider;
import net.minecraft.world.entity.SlotProvider;
import net.minecraft.world.item.ItemProvider;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.slot.SlotCollection;
import net.minecraft.world.item.slot.SlotSource;
public class ItemCommands {
    public static final Dynamic3CommandExceptionType ERROR_TARGET_NOT_A_CONTAINER = new Dynamic3CommandExceptionType((x, y, z) -> Component.translatableEscape("commands.item.target.not_a_container", x, y, z));
    public static final Dynamic3CommandExceptionType ERROR_SOURCE_NOT_A_CONTAINER = new Dynamic3CommandExceptionType((x, y, z) -> Component.translatableEscape("commands.item.source.not_a_container", x, y, z));
    public static final DynamicCommandExceptionType ERROR_TARGET_INAPPLICABLE_SLOT = new DynamicCommandExceptionType(slot -> Component.translatableEscape("commands.item.target.no_such_slot", slot));
    public static final SimpleCommandExceptionType ERROR_TARGET_NO_CHANGES = new SimpleCommandExceptionType(Component.translatable("commands.item.target.failed"));
    public static final DynamicCommandExceptionType ERROR_TARGET_NO_CHANGES_KNOWN_ITEM = new DynamicCommandExceptionType(item -> Component.translatableEscape("commands.item.target.failed.known_item", item));
    public static final List<ArgProvider.Factory<ItemAccessor<?>>> ALL_PROVIDERS = List.of(EntityItemAccessor.PROVIDER, BlockItemAccessor.PROVIDER);
    public static final List<ArgProvider<ItemAccessor<?>>> TARGET_PROVIDERS = ArgProvider.buildList("target", ALL_PROVIDERS);
    public static final List<ArgProvider<ItemAccessor<?>>> SOURCE_PROVIDERS = ArgProvider.buildList("source", ALL_PROVIDERS);
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher, CommandBuildContext context) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "item");
    }
    public static ItemProvider replaceItemProvider(Collection<ItemStack> items) {
        return ItemProvider.of(items);
    }
    public static ItemProvider fillItemProvider(Collection<ItemStack> items) {
        return ItemProvider.cycle(items);
    }
    public static ItemProvider overrideItemProvider(Collection<ItemStack> items) {
        return ItemProvider.of(items).orElseProvide(ItemStack.EMPTY);
    }
    static SlotCollection getSlotsFromProvider(CommandSourceStack source, SlotProvider container, SlotSource slotSource) {
        LootParams params = new LootParams.Builder(source.getLevel())
            .withParameter(LootContextParams.ORIGIN, source.getPosition())
            .withParameter(LootContextParams.CONTAINER, container)
            .withOptionalParameter(LootContextParams.THIS_ENTITY, source.getEntity())
            .create(LootContextParamSets.COMMAND_SLOT_SOURCE);
        LootContext context = new LootContext.Builder(params).create(java.util.Optional.empty());
        context.pushVisitedElement(LootContext.createVisitedEntry(slotSource));
        return slotSource.provide(context);
    }
    interface ItemDistributor {
        ItemProvider apply(Collection<ItemStack> items);
    }
}
