#include "oled_queue.h"

  #include "log.h"


static OLED_Message queueItems[OLED_QUEUE_MAX_ITEMS];
static OLED_Message *current = nullptr;

// ============= INTERNAL HELPERS =============

static bool hasFreeSlot() {
  for (auto &m : queueItems) {
    if (!m.valid) return true;
  }
  return false;
}

static int findSameId(uint8_t id) {
  for (int i = 0; i < OLED_QUEUE_MAX_ITEMS; i++) {
    if (queueItems[i].valid && queueItems[i].id == id) {
      LOG("found same id, overwriting");
      return i;
    }
  }
  return -1;
}

static int findLowestPrioritySlot() {
  int idx = -1;
  uint8_t worst = 0;

  for (int i = 0; i < OLED_QUEUE_MAX_ITEMS; i++) {
    if (!queueItems[i].valid) continue;

    if (queueItems[i].priority > worst) {
      worst = queueItems[i].priority;
      idx = i;
    }
  }

  LOG("computed worst priority slot");
  return idx;
}

static void evictOldWaiting() {
  uint32_t now = millis();

  for (auto &m : queueItems) {
    if (!m.valid) continue;

    if (now - m.enqueuedAtMs > OLED_QUEUE_EXPIRE_WAITING_MS) {
      LOG_MIN("evicting expired waiting message");
      m.valid = false;
    }
  }
}

static void activateMessage(OLED_Message &msg) {
  LOG_MIN("activating message");

  msg.activatedAtMs = millis();
  current = &msg;

  switch (msg.type) {
    case OLED_MSG_TEXT:
      LOG("activate text");
      OLED_DisplayText(msg.text, DEFAULT_TEXT_SIZE);
      break;

    case OLED_MSG_NUMBER:
      LOG("activate number");
      OLED_DisplayNumber(msg.number);
      break;

    case OLED_MSG_PASSWORD:
      LOG("activate password");
      OLED_DisplayPassword(msg.password, false);
      break;
  }
}

// choose next msg based on rules
static OLED_Message* selectNextMessage() {

  LOG("selectNextMessage() scanning");

  OLED_Message *best = nullptr;

  for (auto &m : queueItems) {
    if (!m.valid) continue;

    if (!best) {
      best = &m;
      continue;
    }

    // higher priority wins (lower numeric = higher priority)
    if (m.priority < best->priority) {
      LOG("higher priority candidate found");
      best = &m;
      continue;
    }

    // same priority → FIFO
    if (m.priority == best->priority &&
        m.enqueuedAtMs < best->enqueuedAtMs) {

      LOG("FIFO tiebreaker applied");
      best = &m;
    }
  }

  if (!best) LOG("no next message");
  return best;
}

// ============= PUBLIC API =============

void OLED_QueueInit() {
  LOG_MIN("queue init");

  for (auto &m : queueItems) m.valid = false;
  current = nullptr;
}



void OLED_Refresh() {

  LOG("refresh begin");

  uint32_t now = millis();

  // clean expired waiting messages
  evictOldWaiting();

  // -------------- CASE 1 -----------------
  // nothing active right now
  // ---------------------------------------
  if (!current || !current->valid) {

    LOG_MIN("no active message");

    // if queue empty -> DO NOTHING (keep OLED as-is forever)
    OLED_Message *next = selectNextMessage();
    if (!next) {
      LOG_MIN("queue empty -> leaving screen unchanged");
      return;
    }

    // otherwise display next
    activateMessage(*next);
    return;
  }

  // ------------------------------------------------------
  // HERE: we already have a current valid displayed message
  // ------------------------------------------------------

  // ---------- find the best candidate in queue ----------
  OLED_Message *next = selectNextMessage();

  // if queue empty -> KEEP CURRENT FOREVER
  if (!next) {
    LOG_MIN("queue empty but active exists -> keep forever");
    return;
  }

  // -------------------------------------------
  // check remaining minimum display time
  // -------------------------------------------
  uint32_t elapsed = now - current->activatedAtMs;
  uint32_t minMs = (uint32_t)current->minSeconds * 1000UL;

  // ---------- SAME PRIORITY RULE ----------
  // Same priority MAY switch only if min time satisfied
  if (next->priority == current->priority) {

    if (elapsed < minMs) {
      LOG("same priority but min time not finished -> wait");
      return;
    }

    LOG_MIN("same priority & min time satisfied -> FIFO switch");
    current->valid = false;
    activateMessage(*next);
    return;
  }

  // ---------- HIGHER PRIORITY RULE ----------
  // lower numeric value = higher priority
  if (next->priority < current->priority) {

    LOG_MIN("higher priority message -> preempt immediately");
    current->valid = false;
    activateMessage(*next);
    return;
  }

  // ---------- LOWER PRIORITY RULE ----------
  // never interrupts anything
  LOG("lower priority waiting -> ignored for now");
}




// ============= REQUEST GENERATORS =============

static void enqueueMessage(const OLED_Message &msg) {

  LOG("enqueueMessage called");


// overwrite same ID
int idx = findSameId(msg.id);
if (idx >= 0) {

  OLED_Message &existing = queueItems[idx];

  // --------------------------
  // CASE 1: Same ID is queued
  // --------------------------
  if (&existing != current) {
    LOG_MIN("overwriting queued message with same ID");
    existing = msg;
    existing.valid = true;
    return;
  }

  // --------------------------
  // CASE 2: Same ID is ACTIVE
  // --------------------------
  LOG_MIN("overwriting ACTIVE message with same ID");

  uint32_t now = millis();

  // clamp extra 0–3
  uint8_t extra = msg.extraOnOverwrite;
  if (extra > 3) extra = 3;

  uint32_t elapsed = now - current->activatedAtMs;
  uint32_t minRequired = (uint32_t)current->minSeconds * 1000UL;

  // has the old message already satisfied min time?
  if (elapsed >= minRequired) {
    // treat overwrite as NEW full message
    *current = msg;
    current->activatedAtMs = now;
    activateMessage(*current);
    return;
  }

  // remaining ms to satisfy original min
  uint32_t remaining = minRequired - elapsed;

  // apply extra time extension
  remaining += (uint32_t)extra * 1000UL;

  // update current with new payload
  *current = msg;
  current->activatedAtMs = now - (minRequired - remaining);

  // recompute effective minSeconds based on new remaining time
  current->minSeconds = (remaining + 999UL) / 1000UL;

  activateMessage(*current);
  return;
}


  // free slot available
  for (auto &m : queueItems) {
    if (!m.valid) {
      LOG_MIN("placing message in free slot");
      m = msg;
      m.valid = true;
      return;
    }
  }

  LOG("queue full – trying eviction");

  // try freeing old waiting items
  evictOldWaiting();

  if (hasFreeSlot()) {
    LOG("space appeared after eviction -> retry enqueue");
    enqueueMessage(msg);
    return;
  }

  // evict lower priority if necessary
  int worstIdx = findLowestPrioritySlot();

  if (worstIdx >= 0 &&
      queueItems[worstIdx].priority > msg.priority) {

    LOG_MIN("evicting lower priority message");
    queueItems[worstIdx] = msg;
    queueItems[worstIdx].valid = true;
    return;
  }

  LOG_MIN("dropping message – priority too low");
}

void OLED_RequestDisplayText(
  const String &text,
  uint8_t id,
  uint8_t priority,
  uint8_t minSeconds,
  uint8_t extraOnOverwrite
)
{
  OLED_Message m;
  m.valid = true;
  m.id = id;
  m.priority = priority;
  m.minSeconds = minSeconds;
  m.extraOnOverwrite = extraOnOverwrite;
  m.type = OLED_MSG_TEXT;
  m.text = text;
  m.enqueuedAtMs = millis();

  enqueueMessage(m);
  OLED_Refresh();
}


void OLED_RequestDisplayNumber(
  uint8_t number,
  uint8_t id,
  uint8_t priority,
  uint8_t minSeconds,
   uint8_t extraOnOverwrite
) {
  LOG_MIN("request number message");

  OLED_Message m;
  m.valid = true;
  m.id = id;
  m.priority = priority;
  m.minSeconds = minSeconds;
  m.type = OLED_MSG_NUMBER;
  m.number = number;
  m.enqueuedAtMs = millis();

  enqueueMessage(m);
  OLED_Refresh();
}

void OLED_RequestDisplayPassword(
  const String &password,
  uint8_t id,
  uint8_t priority,
  uint8_t minSeconds,
   uint8_t extraOnOverwrite
) {
  LOG_MIN("request password message");

  OLED_Message m;
  m.valid = true;
  m.id = id;
  m.priority = priority;
  m.minSeconds = minSeconds;
  m.type = OLED_MSG_PASSWORD;
  m.password = password;
  m.enqueuedAtMs = millis();

  enqueueMessage(m);
  OLED_Refresh();
}
