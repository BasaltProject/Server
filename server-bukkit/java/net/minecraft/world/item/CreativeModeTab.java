// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import java.util.Collection;
import java.util.LinkedHashSet;
import java.util.Set;
import java.util.function.Supplier;
import net.minecraft.core.HolderLookup;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
import net.minecraft.world.flag.FeatureFlagSet;
import net.minecraft.world.level.ItemLike;

public class CreativeModeTab {
    private static final Identifier kDefaultBackground = createTextureLocation("items");
    private final Component displayName;
    private Identifier backgroundTexture = kDefaultBackground;
    private boolean canScroll = true;
    private boolean showTitle = true;
    private boolean alignedRight = false;
    private final Row row;
    private final int column;
    private final Type type;
    private ItemStack iconItemStack;
    private Collection<ItemStack> displayItems = new LinkedHashSet<>();
    private Set<ItemStack> displayItemsSearchTab = new LinkedHashSet<>();
    private final Supplier<ItemStack> iconGenerator;
    private final DisplayItemsGenerator displayItemsGenerator;
    private ItemDisplayParameters lastParameters;

    private CreativeModeTab(Row row, int column, Type type, Component displayName, Supplier<ItemStack> iconGenerator, DisplayItemsGenerator displayItemsGenerator) {
        this.row = row;
        this.column = column;
        this.type = type;
        this.displayName = displayName;
        this.iconGenerator = iconGenerator;
        this.displayItemsGenerator = displayItemsGenerator;
    }

    public static Identifier createTextureLocation(String name) {
        return Identifier.withDefaultNamespace("textures/gui/container/creative_inventory/tab_" + name + ".png");
    }

    public static Builder builder(Row row, int column) {
        return new Builder(row, column);
    }

    public Component getDisplayName() {
        return displayName;
    }

    public ItemStack getIconItem() {
        if (iconItemStack == null) {
            iconItemStack = iconGenerator.get();
        }
        return iconItemStack;
    }

    public Identifier getBackgroundTexture() {
        return backgroundTexture;
    }

    public boolean showTitle() {
        return showTitle;
    }

    public boolean canScroll() {
        return canScroll;
    }

    public int column() {
        return column;
    }

    public Row row() {
        return row;
    }

    public boolean hasAnyItems() {
        return !displayItems.isEmpty();
    }

    public boolean shouldDisplay() {
        return type != Type.CATEGORY || hasAnyItems();
    }

    public boolean isAlignedRight() {
        return alignedRight;
    }

    public Type getType() {
        return type;
    }

    public void buildContents(ItemDisplayParameters parameters) {
        ItemDisplayBuilder builder = new ItemDisplayBuilder(this, parameters.enabledFeatures());
        displayItemsGenerator.accept(parameters, builder);
        displayItems = builder.tabContents;
        displayItemsSearchTab = builder.searchTabContents;
        lastParameters = parameters;
    }

    public Collection<ItemStack> getDisplayItems() {
        return displayItems;
    }

    public Collection<ItemStack> getSearchTabDisplayItems() {
        return displayItemsSearchTab;
    }

    public boolean contains(ItemStack stack) {
        return displayItemsSearchTab.stream().anyMatch(candidate -> ItemStack.isSameItemSameComponents(candidate, stack));
    }

    public static record ItemDisplayParameters(FeatureFlagSet enabledFeatures, boolean hasPermissions, HolderLookup.Provider holders) {
        public boolean needsUpdate(FeatureFlagSet enabledFeatures, boolean hasPermissions, HolderLookup.Provider holders) {
            return !this.enabledFeatures.equals(enabledFeatures) || this.hasPermissions != hasPermissions || this.holders != holders;
        }
    }

    public static enum Type {
        CATEGORY, INVENTORY, HOTBAR, SEARCH;
    }

    public static enum Row {
        TOP, BOTTOM;
    }

    public static class Builder {
        private static final DisplayItemsGenerator kEmpty = (parameters, output) -> {};
        private final Row row;
        private final int column;
        private Component displayName = Component.empty();
        private Supplier<ItemStack> iconGenerator = () -> ItemStack.EMPTY;
        private DisplayItemsGenerator displayItemsGenerator = kEmpty;
        private boolean canScroll = true;
        private boolean showTitle = true;
        private boolean alignedRight = false;
        private Type type = Type.CATEGORY;
        private Identifier backgroundTexture = kDefaultBackground;

        public Builder(Row row, int column) {
            this.row = row;
            this.column = column;
        }

        public Builder title(Component displayName) {
            this.displayName = displayName;
            return this;
        }

        public Builder icon(Supplier<ItemStack> iconGenerator) {
            this.iconGenerator = iconGenerator;
            return this;
        }

        public Builder displayItems(DisplayItemsGenerator displayItemsGenerator) {
            this.displayItemsGenerator = displayItemsGenerator;
            return this;
        }

        public Builder alignedRight() {
            alignedRight = true;
            return this;
        }

        public Builder hideTitle() {
            showTitle = false;
            return this;
        }

        public Builder noScrollBar() {
            canScroll = false;
            return this;
        }

        protected Builder type(Type type) {
            this.type = type;
            return this;
        }

        public Builder backgroundTexture(Identifier backgroundTexture) {
            this.backgroundTexture = backgroundTexture;
            return this;
        }

        public CreativeModeTab build() {
            CreativeModeTab tab = new CreativeModeTab(row, column, type, displayName, iconGenerator, displayItemsGenerator);
            tab.alignedRight = alignedRight;
            tab.showTitle = showTitle;
            tab.canScroll = canScroll;
            tab.backgroundTexture = backgroundTexture;
            return tab;
        }
    }

    static class ItemDisplayBuilder implements Output {
        public final Collection<ItemStack> tabContents = new LinkedHashSet<>();
        public final Set<ItemStack> searchTabContents = new LinkedHashSet<>();
        private final CreativeModeTab tab;
        private final FeatureFlagSet featureFlagSet;

        public ItemDisplayBuilder(CreativeModeTab tab, FeatureFlagSet featureFlagSet) {
            this.tab = tab;
            this.featureFlagSet = featureFlagSet;
        }

        public void accept(ItemStack stack, TabVisibility tabVisibility) {
            if (stack.getCount() != 1) {
                throw new IllegalArgumentException("Stack size must be exactly 1");
            }
            boolean duplicate = tabContents.stream().anyMatch(existing -> ItemStack.isSameItemSameComponents(existing, stack));
            if (duplicate && tabVisibility != TabVisibility.SEARCH_TAB_ONLY) {
                throw new IllegalStateException("Accidentally adding the same item stack twice to a Creative Mode Tab: " + stack.getDisplayName().getString());
            }
            if (tabVisibility != TabVisibility.SEARCH_TAB_ONLY) {
                tabContents.add(stack);
            }
            if (tabVisibility != TabVisibility.PARENT_TAB_ONLY) {
                searchTabContents.add(stack);
            }
        }
    }

    protected static enum TabVisibility {
        PARENT_AND_SEARCH_TABS, PARENT_TAB_ONLY, SEARCH_TAB_ONLY;
    }

    protected interface Output {
        void accept(ItemStack stack, TabVisibility tabVisibility);

        default void accept(ItemStack stack) {
            accept(stack, TabVisibility.PARENT_AND_SEARCH_TABS);
        }

        default void accept(ItemLike item, TabVisibility tabVisibility) {
            accept(new ItemStack(item), tabVisibility);
        }

        default void accept(ItemLike item) {
            accept(new ItemStack(item), TabVisibility.PARENT_AND_SEARCH_TABS);
        }

        default void acceptAll(Collection<ItemStack> stacks, TabVisibility tabVisibility) {
            stacks.forEach(stack -> accept(stack, tabVisibility));
        }

        default void acceptAll(Collection<ItemStack> stacks) {
            acceptAll(stacks, TabVisibility.PARENT_AND_SEARCH_TABS);
        }
    }

    public interface DisplayItemsGenerator {
        void accept(ItemDisplayParameters parameters, Output output);
    }
}
