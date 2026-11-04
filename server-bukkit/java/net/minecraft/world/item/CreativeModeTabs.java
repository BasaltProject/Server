// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.Registry;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
import net.minecraft.world.flag.FeatureFlagSet;

public class CreativeModeTabs {
    private static final List<CreativeModeTab> kTabs = new ArrayList<>();
    private static CreativeModeTab kDefault;
    private static CreativeModeTab kSearch;
    private static CreativeModeTab.ItemDisplayParameters kParameters;

    private static void add(Registry<CreativeModeTab> registry, String name, CreativeModeTab.Builder builder) {
        CreativeModeTab tab = builder.build();
        Registry.register(registry, name, tab);
        kTabs.add(tab);
    }

    private static CreativeModeTab.Builder top(int column, String title, java.util.function.Supplier<ItemStack> icon) {
        return CreativeModeTab.builder(CreativeModeTab.Row.TOP, column).title(Component.translatable(title)).icon(icon);
    }

    private static CreativeModeTab.Builder bottom(int column, String title, java.util.function.Supplier<ItemStack> icon) {
        return CreativeModeTab.builder(CreativeModeTab.Row.BOTTOM, column).title(Component.translatable(title)).icon(icon);
    }

    public static synchronized CreativeModeTab bootstrap(Registry<CreativeModeTab> registry) {
        if (!kTabs.isEmpty()) {
            return kDefault;
        }
        add(registry, "building_blocks", top(0, "itemGroup.buildingBlocks", () -> new ItemStack(Items.BRICKS)));
        add(registry, "colored_blocks", top(1, "itemGroup.coloredBlocks", () -> new ItemStack(BuiltInRegistries.ITEM.getValue(Identifier.withDefaultNamespace("white_wool")))));
        add(registry, "natural_blocks", top(2, "itemGroup.natural", () -> new ItemStack(Items.GRASS_BLOCK)));
        add(registry, "functional_blocks", top(3, "itemGroup.functional", () -> new ItemStack(Items.OAK_SIGN)));
        add(registry, "redstone_blocks", top(4, "itemGroup.redstone", () -> new ItemStack(Items.REDSTONE)));
        add(registry, "hotbar", top(5, "itemGroup.hotbar", () -> new ItemStack(Items.BOOKSHELF)).alignedRight().type(CreativeModeTab.Type.HOTBAR));
        add(registry, "search", top(6, "itemGroup.search", () -> new ItemStack(Items.COMPASS))
            .displayItems((parameters, output) -> BuiltInRegistries.ITEM.stream().forEach(item -> output.accept(item, CreativeModeTab.TabVisibility.SEARCH_TAB_ONLY)))
            .backgroundTexture(CreativeModeTab.createTextureLocation("item_search")).alignedRight().type(CreativeModeTab.Type.SEARCH));
        add(registry, "tools_and_utilities", bottom(0, "itemGroup.tools", () -> new ItemStack(Items.DIAMOND_PICKAXE)));
        add(registry, "combat", bottom(1, "itemGroup.combat", () -> new ItemStack(Items.NETHERITE_SWORD)));
        add(registry, "food_and_drinks", bottom(2, "itemGroup.foodAndDrink", () -> new ItemStack(Items.GOLDEN_APPLE)));
        add(registry, "ingredients", bottom(3, "itemGroup.ingredients", () -> new ItemStack(Items.IRON_INGOT)));
        add(registry, "spawn_eggs", bottom(4, "itemGroup.spawnEggs", () -> new ItemStack(Items.CREEPER_SPAWN_EGG)));
        add(registry, "op_blocks", bottom(5, "itemGroup.op", () -> new ItemStack(Items.COMMAND_BLOCK)).alignedRight());
        add(registry, "inventory", bottom(6, "itemGroup.inventory", () -> new ItemStack(Items.CHEST))
            .backgroundTexture(CreativeModeTab.createTextureLocation("inventory")).alignedRight().type(CreativeModeTab.Type.INVENTORY));
        kDefault = kTabs.get(0);
        kSearch = kTabs.get(6);
        return kDefault;
    }

    public static void validate() {
        for (CreativeModeTab tab : kTabs) {
            for (CreativeModeTab other : kTabs) {
                if (tab != other && tab.row() == other.row() && tab.column() == other.column()) {
                    throw new IllegalStateException("Duplicate position in creative tabs: " + tab.getDisplayName().getString());
                }
            }
        }
    }

    public static CreativeModeTab getDefaultTab() {
        return kDefault;
    }

    public static List<CreativeModeTab> tabs() {
        return kTabs.stream().filter(tab -> tab.getType() == CreativeModeTab.Type.CATEGORY).toList();
    }

    public static List<CreativeModeTab> allTabs() {
        return List.copyOf(kTabs);
    }

    public static CreativeModeTab searchTab() {
        return kSearch;
    }

    public static boolean tryRebuildTabContents(FeatureFlagSet enabledFeatures, boolean hasPermissions, HolderLookup.Provider lookup) {
        if (kParameters != null && !kParameters.needsUpdate(enabledFeatures, hasPermissions, lookup)) {
            return false;
        }
        kParameters = new CreativeModeTab.ItemDisplayParameters(enabledFeatures, hasPermissions, lookup);
        for (CreativeModeTab tab : kTabs) {
            tab.buildContents(kParameters);
        }
        return true;
    }
}
