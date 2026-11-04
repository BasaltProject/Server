// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.flag.FeatureFlagSet;
import net.minecraft.world.flag.FeatureElement;

public class MenuType<T extends AbstractContainerMenu> implements FeatureElement {
    public static final MenuType<ChestMenu> GENERIC_9x1 = new MenuType<>((id, inv) -> ChestMenu.oneRow(id, inv));
    public static final MenuType<ChestMenu> GENERIC_9x2 = new MenuType<>((id, inv) -> ChestMenu.twoRows(id, inv));
    public static final MenuType<ChestMenu> GENERIC_9x3 = new MenuType<>((id, inv) -> ChestMenu.threeRows(id, inv));
    public static final MenuType<ChestMenu> GENERIC_9x4 = new MenuType<>((id, inv) -> ChestMenu.fourRows(id, inv));
    public static final MenuType<ChestMenu> GENERIC_9x5 = new MenuType<>((id, inv) -> ChestMenu.fiveRows(id, inv));
    public static final MenuType<ChestMenu> GENERIC_9x6 = new MenuType<>((id, inv) -> ChestMenu.sixRows(id, inv));
    public static final MenuType<DispenserMenu> GENERIC_3x3 = new MenuType<>(DispenserMenu::new);
    public static final MenuType<FurnaceMenu> FURNACE = new MenuType<>(FurnaceMenu::new);
    public static final MenuType<BlastFurnaceMenu> BLAST_FURNACE = new MenuType<>(BlastFurnaceMenu::new);
    public static final MenuType<SmokerMenu> SMOKER = new MenuType<>(SmokerMenu::new);
    public static final MenuType<HopperMenu> HOPPER = new MenuType<>(HopperMenu::new);
    public static final MenuType<ShulkerBoxMenu> SHULKER_BOX = new MenuType<>(ShulkerBoxMenu::new);

    public static final MenuType<EnchantmentMenu> ENCHANTMENT = new MenuType<>(EnchantmentMenu::new);
    public static final MenuType<GrindstoneMenu> GRINDSTONE = new MenuType<>(GrindstoneMenu::new);
    public static final MenuType<StonecutterMenu> STONECUTTER = new MenuType<>(StonecutterMenu::new);
    public static final MenuType<CartographyTableMenu> CARTOGRAPHY_TABLE = new MenuType<>(CartographyTableMenu::new);
    public static final MenuType<LoomMenu> LOOM = new MenuType<>(LoomMenu::new);

    public static final MenuType<BrewingStandMenu> BREWING_STAND = new MenuType<>(BrewingStandMenu::new);
    public static final MenuType<BeaconMenu> BEACON = new MenuType<>((id, inv) -> new BeaconMenu(id, inv));

    public static final MenuType<LecternMenu> LECTERN = new MenuType<>((id, inv) -> new LecternMenu(id));
    public static final MenuType<MerchantMenu> MERCHANT = new MenuType<>(MerchantMenu::new);
    public static final MenuType<AnvilMenu> ANVIL = new MenuType<>(AnvilMenu::new);
    public static final MenuType<SmithingMenu> SMITHING = new MenuType<>(SmithingMenu::new);

    public static final MenuType<CrafterMenu> CRAFTER_3x3 = new MenuType<>(CrafterMenu::new);
    public static final MenuType<CraftingMenu> CRAFTING = new MenuType<>(CraftingMenu::new);

    private final MenuSupplier<T> constructor;

    private MenuType(MenuSupplier<T> constructor) {
        this.constructor = constructor;
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
