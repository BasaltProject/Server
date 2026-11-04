// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package com.mojang.authlib.services.response;

import java.util.UUID;

public record NameAndId(UUID id, String name) {
}
