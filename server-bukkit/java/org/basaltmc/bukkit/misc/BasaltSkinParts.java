package org.basaltmc.bukkit.misc;

import com.destroystokyo.paper.SkinParts;

public final class BasaltSkinParts implements SkinParts.Mutable {
    private int raw;

    public BasaltSkinParts(int raw) {
        this.raw = raw & 0x7F;
    }

    private boolean has(int bit) {
        return (raw & bit) != 0;
    }

    private void set(int bit, boolean on) {
        raw = on ? raw | bit : raw & ~bit;
    }

    @Override
    public boolean hasCapeEnabled() {
        return has(1);
    }

    @Override
    public boolean hasJacketEnabled() {
        return has(2);
    }

    @Override
    public boolean hasLeftSleeveEnabled() {
        return has(4);
    }

    @Override
    public boolean hasRightSleeveEnabled() {
        return has(8);
    }

    @Override
    public boolean hasLeftPantsEnabled() {
        return has(16);
    }

    @Override
    public boolean hasRightPantsEnabled() {
        return has(32);
    }

    @Override
    public boolean hasHatsEnabled() {
        return has(64);
    }

    @Override
    public int getRaw() {
        return raw;
    }

    @Override
    public Mutable mutableCopy() {
        return new BasaltSkinParts(raw);
    }

    @Override
    public void setCapeEnabled(boolean enabled) {
        set(1, enabled);
    }

    @Override
    public void setJacketEnabled(boolean enabled) {
        set(2, enabled);
    }

    @Override
    public void setLeftSleeveEnabled(boolean enabled) {
        set(4, enabled);
    }

    @Override
    public void setRightSleeveEnabled(boolean enabled) {
        set(8, enabled);
    }

    @Override
    public void setLeftPantsEnabled(boolean enabled) {
        set(16, enabled);
    }

    @Override
    public void setRightPantsEnabled(boolean enabled) {
        set(32, enabled);
    }

    @Override
    public void setHatsEnabled(boolean enabled) {
        set(64, enabled);
    }

    @Override
    public SkinParts immutableCopy() {
        return new BasaltSkinParts(raw);
    }
}
