package org.basaltmc.server.runtime

import io.ktor.client.HttpClient
import io.ktor.client.request.get
import io.ktor.client.statement.HttpResponse
import io.ktor.client.statement.bodyAsText
import io.ktor.http.HttpStatusCode
import kotlinx.coroutines.runBlocking
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive

/**
 * Name to UUID against Mojang's public profile API, which is what `/fetchprofile` answers with.
 *
 * The dashless form the API returns is expanded, because every other UUID this server prints is
 * dashed and two spellings of the same id in the same console is a needless puzzle.
 */
public object MojangProfileLookup {
    public fun uuidOf(
        name: String,
        client: HttpClient = HttpClient(),
    ): String? =
        runBlocking {
            val response: HttpResponse = client.get("https://api.mojang.com/users/profiles/minecraft/$name")
            if (response.status != HttpStatusCode.OK) return@runBlocking null
            val body = Json.parseToJsonElement(response.bodyAsText()) as? JsonObject ?: return@runBlocking null
            val id = (body["id"] as? JsonPrimitive)?.content ?: return@runBlocking null
            dashed(id)
        }

    private fun dashed(id: String): String =
        if (id.length != 32) {
            id
        } else {
            buildString {
                append(id, 0, 8)
                append('-')
                append(id, 8, 12)
                append('-')
                append(id, 12, 16)
                append('-')
                append(id, 16, 20)
                append('-')
                append(id, 20, 32)
            }
        }
}
