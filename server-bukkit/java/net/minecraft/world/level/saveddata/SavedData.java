// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.saveddata;
public abstract class SavedData {
    private boolean dirty;
    public void setDirty() {
        setDirty(true);
    }
    public void setDirty(boolean dirty) {
        this.dirty = dirty;
    }
    public boolean isDirty() {
        return dirty;


    }
}
