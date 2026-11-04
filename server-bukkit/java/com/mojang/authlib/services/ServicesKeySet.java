// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package com.mojang.authlib.services;

import java.security.PublicKey;
import java.util.Collection;

public interface ServicesKeySet {
    Collection<PublicKey> keys(ServicesKeyType type);
}
