package org.basaltmc.bukkit.facade;

public final class NmsStub {
    private NmsStub() {}

    @SuppressWarnings("unchecked")
    public static <T> T nul() {
        return null;
    }

    public static UnsupportedOperationException unsupported(String member) {
        return new UnsupportedOperationException(member + " is not implemented by Basalt");
    }
}
