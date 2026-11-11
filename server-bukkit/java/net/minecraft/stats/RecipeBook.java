// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.stats;

import net.minecraft.world.inventory.RecipeBookType;

public class RecipeBook {
    protected final RecipeBookSettings bookSettings = new RecipeBookSettings();

    public boolean isOpen(RecipeBookType recipeBookType) {
        return bookSettings.isOpen(recipeBookType);
    }

    public void setOpen(RecipeBookType recipeBookType, boolean open) {
        bookSettings.setOpen(recipeBookType, open);
    }

    public boolean isFiltering(RecipeBookType type) {
        return bookSettings.isFiltering(type);
    }

    public void setFiltering(RecipeBookType type, boolean filtering) {
        bookSettings.setFiltering(type, filtering);
    }

    public void setBookSettings(RecipeBookSettings settings) {
        bookSettings.replaceFrom(settings);
    }

    public RecipeBookSettings getBookSettings() {
        return bookSettings;
    }

    public void setBookSetting(RecipeBookType bookType, boolean open, boolean filtering) {
        bookSettings.setOpen(bookType, open);
        bookSettings.setFiltering(bookType, filtering);
    }
}
