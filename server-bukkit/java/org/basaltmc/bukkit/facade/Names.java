package org.basaltmc.bukkit.facade;

/** Resource names as the data fixers read them. */
public final class Names {
    private Names() {}

    /** The name with the default namespace added; text that is not a valid name comes back as it was. */
    public static String namespaced(String input) {
        int colon = input.indexOf(':');
        String namespace = colon < 0 ? "minecraft" : input.substring(0, colon);
        String path = colon < 0 ? input : input.substring(colon + 1);
        if (namespace.isEmpty()) {
            namespace = "minecraft";
        }
        return validNamespace(namespace) && validPath(path) ? namespace + ":" + path : input;
    }

    private static boolean validNamespace(String text) {
        for (int i = 0; i < text.length(); i++) {
            char c = text.charAt(i);
            if (!(c >= 'a' && c <= 'z' || c >= '0' && c <= '9' || c == '_' || c == '-' || c == '.')) {
                return false;
            }
        }
        return true;
    }

    private static boolean validPath(String text) {
        for (int i = 0; i < text.length(); i++) {
            char c = text.charAt(i);
            if (!(c >= 'a' && c <= 'z' || c >= '0' && c <= '9' || c == '_' || c == '-' || c == '.' || c == '/')) {
                return false;
            }
        }
        return true;
    }
}
