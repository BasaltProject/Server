// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.flag.FeatureFlagSet;
import net.minecraft.world.flag.FeatureElement;

public class MenuType<T extends AbstractContainerMenu> implements FeatureElement {
    public static final MenuType<ChestMenu> GENERIC_9x1 = register("generic_9x1", (id, inv) -> ChestMenu.oneRow(id, inv));
    public static final MenuType<ChestMenu> GENERIC_9x2 = register("generic_9x2", (id, inv) -> ChestMenu.twoRows(id, inv));
    public static final MenuType<ChestMenu> GENERIC_9x3 = register("generic_9x3", (id, inv) -> ChestMenu.threeRows(id, inv));
    public static final MenuType<ChestMenu> GENERIC_9x4 = register("generic_9x4", (id, inv) -> ChestMenu.fourRows(id, inv));
    public static final MenuType<ChestMenu> GENERIC_9x5 = register("generic_9x5", (id, inv) -> ChestMenu.fiveRows(id, inv));
    public static final MenuType<ChestMenu> GENERIC_9x6 = register("generic_9x6", (id, inv) -> ChestMenu.sixRows(id, inv));
    public static final MenuType<DispenserMenu> GENERIC_3x3 = register("generic_3x3", DispenserMenu::new);
    public static final MenuType<CrafterMenu> CRAFTER_3x3 = register("crafter_3x3", CrafterMenu::new);
    public static final MenuType<AnvilMenu> ANVIL = register("anvil", AnvilMenu::new);
    public static final MenuType<BeaconMenu> BEACON = register("beacon", (id, inv) -> new BeaconMenu(id, inv));
    public static final MenuType<BlastFurnaceMenu> BLAST_FURNACE = register("blast_furnace", BlastFurnaceMenu::new);
    public static final MenuType<BrewingStandMenu> BREWING_STAND = register("brewing_stand", BrewingStandMenu::new);
    public static final MenuType<CraftingMenu> CRAFTING = register("crafting", CraftingMenu::new);
    public static final MenuType<EnchantmentMenu> ENCHANTMENT = register("enchantment", EnchantmentMenu::new);
    public static final MenuType<FurnaceMenu> FURNACE = register("furnace", FurnaceMenu::new);
    public static final MenuType<GrindstoneMenu> GRINDSTONE = register("grindstone", GrindstoneMenu::new);
    public static final MenuType<HopperMenu> HOPPER = register("hopper", HopperMenu::new);
    public static final MenuType<LecternMenu> LECTERN = register("lectern", (id, inv) -> new LecternMenu(id));
    public static final MenuType<LoomMenu> LOOM = register("loom", LoomMenu::new);
    public static final MenuType<MerchantMenu> MERCHANT = register("merchant", MerchantMenu::new);
    public static final MenuType<ShulkerBoxMenu> SHULKER_BOX = register("shulker_box", ShulkerBoxMenu::new);
    public static final MenuType<SmithingMenu> SMITHING = register("smithing", SmithingMenu::new);
    public static final MenuType<SmokerMenu> SMOKER = register("smoker", SmokerMenu::new);
    public static final MenuType<CartographyTableMenu> CARTOGRAPHY_TABLE = register("cartography_table", CartographyTableMenu::new);
    public static final MenuType<StonecutterMenu> STONECUTTER = register("stonecutter", StonecutterMenu::new);

    private final MenuSupplier<T> constructor;

    private MenuType(MenuSupplier<T> constructor) {
        this.constructor = constructor;
    }

    private static <T extends AbstractContainerMenu> MenuType<T> register(String name, MenuSupplier<T> constructor) {
        return net.minecraft.core.Registry.register(net.minecraft.core.registries.BuiltInRegistries.MENU, name,
            new MenuType<>(constructor));
    }

    public T create(int containerId, Inventory inventory) {
        return constructor.create(containerId, inventory);
    }

    public FeatureFlagSet requiredFeatures() {
        return FeatureFlagSet.of();
    }

    interface MenuSupplier<T extends AbstractContainerMenu> {
        T create(int containerId, Inventory inventory);
    }
}
