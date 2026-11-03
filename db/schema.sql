CREATE TABLE "schema_migrations" (version varchar(128) primary key);
CREATE TABLE auth_user (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    username       TEXT    NOT NULL UNIQUE,
    password       TEXT    NOT NULL,
    name           TEXT    NOT NULL DEFAULT '',
    email          TEXT    NOT NULL DEFAULT '',
    last_logged_in INTEGER,
    created_at     INTEGER NOT NULL DEFAULT (unixepoch())
);
CREATE TABLE order_number (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    year       INTEGER NOT NULL,
    seq        INTEGER NOT NULL CHECK (seq > 0),
    family     TEXT    NOT NULL CHECK (family IN ('rent', 'production')),
    created_at INTEGER NOT NULL DEFAULT (unixepoch()),
    UNIQUE (year, seq),
    UNIQUE (id, family)
);
CREATE TABLE party_client (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT    NOT NULL,
    ruc        TEXT    NOT NULL UNIQUE,
    address    TEXT    NOT NULL DEFAULT '',
    district   TEXT    NOT NULL DEFAULT '',
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);
CREATE TABLE party_issuer (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT    NOT NULL,
    ruc        TEXT    NOT NULL UNIQUE,
    address    TEXT    NOT NULL DEFAULT '',
    district   TEXT    NOT NULL DEFAULT '',
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);
CREATE TABLE party_contact (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    client_id  INTEGER REFERENCES party_client (id),
    issuer_id  INTEGER REFERENCES party_issuer (id),
    name       TEXT    NOT NULL,
    phone      TEXT    NOT NULL DEFAULT '',
    role       TEXT    NOT NULL DEFAULT '',
    email      TEXT    NOT NULL DEFAULT '',
    created_at INTEGER NOT NULL DEFAULT (unixepoch()),
    CHECK ((client_id IS NULL) <> (issuer_id IS NULL)),
    UNIQUE (id, client_id),
    UNIQUE (id, issuer_id)
);
CREATE TABLE rentorder_contract_kind (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    code       TEXT    NOT NULL UNIQUE,
    name       TEXT    NOT NULL,
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);
CREATE TABLE rentorder_header (
    id                     INTEGER PRIMARY KEY AUTOINCREMENT,
    order_number_id        INTEGER NOT NULL UNIQUE,
    order_family           TEXT    NOT NULL DEFAULT 'rent' CHECK (order_family = 'rent'),
    order_contract_kind_id INTEGER NOT NULL REFERENCES rentorder_contract_kind (id),
    issued_on              INTEGER NOT NULL,
    client_id              INTEGER NOT NULL REFERENCES party_client (id),
    brand                  TEXT    NOT NULL DEFAULT '',
    contact_id             INTEGER,
    issuer_id              INTEGER NOT NULL REFERENCES party_issuer (id),
    issuer_contact_id      INTEGER,
    electric_power         INTEGER NOT NULL DEFAULT 0 CHECK (electric_power IN (0, 1)),
    guarantee              INTEGER NOT NULL DEFAULT 0 CHECK (guarantee IN (0, 1)),
    guarantee_amount       INTEGER CHECK (guarantee_amount >= 0),
    guarantee_conditions   TEXT    NOT NULL DEFAULT '',
    billing_client_id      INTEGER REFERENCES party_client (id),
    billing_contact_id     INTEGER,
    payment_terms          TEXT    NOT NULL DEFAULT '',
    agency_client_id       INTEGER REFERENCES party_client (id),
    observations           TEXT    NOT NULL DEFAULT '',
    terms_ad_space         TEXT    NOT NULL DEFAULT '',
    terms_temp_space       TEXT    NOT NULL DEFAULT '',
    penalties              TEXT    NOT NULL DEFAULT '',
    other_conditions       TEXT    NOT NULL DEFAULT '',
    resolution             TEXT    NOT NULL DEFAULT '',
    igv_percent            INTEGER NOT NULL DEFAULT 18 CHECK (igv_percent >= 0),
    created_by             INTEGER REFERENCES auth_user (id),
    created_at             INTEGER NOT NULL DEFAULT (unixepoch()),
    FOREIGN KEY (order_number_id, order_family) REFERENCES order_number (id, family),
    FOREIGN KEY (contact_id, client_id) REFERENCES party_contact (id, client_id),
    FOREIGN KEY (issuer_contact_id, issuer_id) REFERENCES party_contact (id, issuer_id),
    FOREIGN KEY (billing_contact_id, billing_client_id) REFERENCES party_contact (id, client_id),
    CHECK (billing_contact_id IS NULL OR billing_client_id IS NOT NULL)
);
CREATE TABLE rentorder_ad_space_line (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    header_id   INTEGER NOT NULL REFERENCES rentorder_header (id) ON DELETE CASCADE,
    position    INTEGER NOT NULL,
    code        TEXT    NOT NULL DEFAULT '',
    element     TEXT    NOT NULL DEFAULT '',
    location    TEXT    NOT NULL DEFAULT '',
    motive      TEXT    NOT NULL DEFAULT '',
    start_date  INTEGER,
    end_date    INTEGER,
    element_qty INTEGER NOT NULL DEFAULT 1 CHECK (element_qty >= 0),
    periods     INTEGER NOT NULL DEFAULT 1 CHECK (periods >= 0),
    period_unit TEXT    NOT NULL DEFAULT 'month' CHECK (period_unit IN ('month', 'day')),
    unit_price  INTEGER NOT NULL DEFAULT 0 CHECK (unit_price >= 0),
    total       INTEGER NOT NULL DEFAULT 0 CHECK (total >= 0),
    UNIQUE (header_id, position)
);
CREATE TABLE rentorder_temp_space_line (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    header_id   INTEGER NOT NULL REFERENCES rentorder_header (id) ON DELETE CASCADE,
    position    INTEGER NOT NULL,
    code        TEXT    NOT NULL DEFAULT '',
    location    TEXT    NOT NULL DEFAULT '',
    area        TEXT    NOT NULL DEFAULT '',
    motive      TEXT    NOT NULL DEFAULT '',
    description TEXT    NOT NULL DEFAULT '',
    schedule    TEXT    NOT NULL DEFAULT '',
    start_date  INTEGER,
    end_date    INTEGER,
    days        INTEGER NOT NULL DEFAULT 1 CHECK (days >= 0),
    total       INTEGER NOT NULL DEFAULT 0 CHECK (total >= 0),
    UNIQUE (header_id, position)
);
CREATE TABLE productionorder_contract_kind (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    code       TEXT    NOT NULL UNIQUE,
    name       TEXT    NOT NULL,
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);
CREATE TABLE productionorder_header (
    id                     INTEGER PRIMARY KEY AUTOINCREMENT,
    order_number_id        INTEGER NOT NULL UNIQUE,
    order_family           TEXT    NOT NULL DEFAULT 'production' CHECK (order_family = 'production'),
    order_contract_kind_id INTEGER NOT NULL REFERENCES productionorder_contract_kind (id),
    issued_on              INTEGER NOT NULL,
    client_id              INTEGER NOT NULL REFERENCES party_client (id),
    brand                  TEXT    NOT NULL DEFAULT '',
    contact_id             INTEGER,
    issuer_id              INTEGER NOT NULL REFERENCES party_issuer (id),
    issuer_contact_id      INTEGER,
    billing_client_id      INTEGER REFERENCES party_client (id),
    billing_contact_id     INTEGER,
    payment_terms          TEXT    NOT NULL DEFAULT '',
    observations           TEXT    NOT NULL DEFAULT '',
    penalties              TEXT    NOT NULL DEFAULT '',
    other_conditions       TEXT    NOT NULL DEFAULT '',
    resolution             TEXT    NOT NULL DEFAULT '',
    igv_percent            INTEGER NOT NULL DEFAULT 18 CHECK (igv_percent >= 0),
    created_by             INTEGER REFERENCES auth_user (id),
    created_at             INTEGER NOT NULL DEFAULT (unixepoch()),
    FOREIGN KEY (order_number_id, order_family) REFERENCES order_number (id, family),
    FOREIGN KEY (contact_id, client_id) REFERENCES party_contact (id, client_id),
    FOREIGN KEY (issuer_contact_id, issuer_id) REFERENCES party_contact (id, issuer_id),
    FOREIGN KEY (billing_contact_id, billing_client_id) REFERENCES party_contact (id, client_id),
    CHECK (billing_contact_id IS NULL OR billing_client_id IS NOT NULL)
);
CREATE TABLE productionorder_line (
    id        INTEGER PRIMARY KEY AUTOINCREMENT,
    header_id INTEGER NOT NULL REFERENCES productionorder_header (id) ON DELETE CASCADE,
    position  INTEGER NOT NULL,
    code      TEXT    NOT NULL DEFAULT '',
    location  TEXT    NOT NULL DEFAULT '',
    element   TEXT    NOT NULL DEFAULT '',
    material  TEXT    NOT NULL DEFAULT '',
    motive    TEXT    NOT NULL DEFAULT '',
    quantity  INTEGER NOT NULL DEFAULT 1 CHECK (quantity >= 0),
    width     REAL,
    height    REAL,
    total     INTEGER NOT NULL DEFAULT 0 CHECK (total >= 0),
    UNIQUE (header_id, position)
);
CREATE TABLE form_block (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    code       TEXT    NOT NULL UNIQUE,
    title      TEXT    NOT NULL,
    active     INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);
CREATE TABLE form_block_family (
    id                          INTEGER PRIMARY KEY AUTOINCREMENT,
    block_id                    INTEGER NOT NULL REFERENCES form_block (id) ON DELETE CASCADE,
    family                      TEXT    NOT NULL CHECK (family IN ('rent', 'production')),
    rent_contract_kind_id       INTEGER REFERENCES rentorder_contract_kind (id),
    production_contract_kind_id INTEGER REFERENCES productionorder_contract_kind (id),
    position                    INTEGER NOT NULL DEFAULT 0,
    active                      INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    created_at                  INTEGER NOT NULL DEFAULT (unixepoch()),
    CHECK (rent_contract_kind_id IS NULL OR family = 'rent'),
    CHECK (production_contract_kind_id IS NULL OR family = 'production'),
    UNIQUE (block_id, family)
);
CREATE TABLE form_field (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    block_id   INTEGER NOT NULL REFERENCES form_block (id) ON DELETE CASCADE,
    code       TEXT    NOT NULL,
    label      TEXT    NOT NULL,
    type       TEXT    NOT NULL CHECK (type IN ('number', 'text', 'option', 'multi_option', 'date', 'boolean')),
    required   INTEGER NOT NULL DEFAULT 0 CHECK (required IN (0, 1)),
    position   INTEGER NOT NULL DEFAULT 0,
    active     INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    created_at INTEGER NOT NULL DEFAULT (unixepoch()),
    UNIQUE (block_id, code),
    UNIQUE (id, type),
    UNIQUE (id, block_id, type)
);
CREATE TABLE form_field_option (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    field_id   INTEGER NOT NULL,
    field_type TEXT    NOT NULL CHECK (field_type IN ('option', 'multi_option')),
    code       TEXT    NOT NULL,
    label      TEXT    NOT NULL,
    position   INTEGER NOT NULL DEFAULT 0,
    active     INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    created_at INTEGER NOT NULL DEFAULT (unixepoch()),
    FOREIGN KEY (field_id, field_type) REFERENCES form_field (id, type) ON DELETE CASCADE,
    UNIQUE (field_id, code),
    UNIQUE (id, field_id)
);
CREATE TABLE rentorder_field_value (
    header_id     INTEGER NOT NULL REFERENCES rentorder_header (id) ON DELETE CASCADE,
    field_id      INTEGER NOT NULL,
    block_id      INTEGER NOT NULL,
    block_family  TEXT    NOT NULL DEFAULT 'rent' CHECK (block_family = 'rent'),
    field_type    TEXT    NOT NULL,
    value_number  REAL,
    value_text    TEXT,
    value_date    INTEGER,
    value_boolean INTEGER CHECK (value_boolean IN (0, 1)),
    option_id     INTEGER,
    created_at    INTEGER NOT NULL DEFAULT (unixepoch()),
    PRIMARY KEY (header_id, field_id),
    UNIQUE (header_id, field_id, field_type),
    FOREIGN KEY (field_id, block_id, field_type) REFERENCES form_field (id, block_id, type),
    FOREIGN KEY (block_id, block_family) REFERENCES form_block_family (block_id, family),
    FOREIGN KEY (option_id, field_id) REFERENCES form_field_option (id, field_id),
    CHECK ((value_number IS NOT NULL) + (value_text IS NOT NULL) + (value_date IS NOT NULL)
           + (value_boolean IS NOT NULL) + (option_id IS NOT NULL)
           = CASE field_type WHEN 'multi_option' THEN 0 ELSE 1 END),
    CHECK (CASE field_type
               WHEN 'number'  THEN value_number IS NOT NULL
               WHEN 'text'    THEN value_text IS NOT NULL
               WHEN 'date'    THEN value_date IS NOT NULL
               WHEN 'boolean' THEN value_boolean IS NOT NULL
               WHEN 'option'  THEN option_id IS NOT NULL
               ELSE 1
           END)
);
CREATE TABLE rentorder_field_value_option (
    header_id  INTEGER NOT NULL,
    field_id   INTEGER NOT NULL,
    field_type TEXT    NOT NULL DEFAULT 'multi_option' CHECK (field_type = 'multi_option'),
    option_id  INTEGER NOT NULL,
    PRIMARY KEY (header_id, field_id, option_id),
    FOREIGN KEY (header_id, field_id, field_type)
        REFERENCES rentorder_field_value (header_id, field_id, field_type) ON DELETE CASCADE,
    FOREIGN KEY (option_id, field_id) REFERENCES form_field_option (id, field_id)
);
CREATE TABLE productionorder_field_value (
    header_id     INTEGER NOT NULL REFERENCES productionorder_header (id) ON DELETE CASCADE,
    field_id      INTEGER NOT NULL,
    block_id      INTEGER NOT NULL,
    block_family  TEXT    NOT NULL DEFAULT 'production' CHECK (block_family = 'production'),
    field_type    TEXT    NOT NULL,
    value_number  REAL,
    value_text    TEXT,
    value_date    INTEGER,
    value_boolean INTEGER CHECK (value_boolean IN (0, 1)),
    option_id     INTEGER,
    created_at    INTEGER NOT NULL DEFAULT (unixepoch()),
    PRIMARY KEY (header_id, field_id),
    UNIQUE (header_id, field_id, field_type),
    FOREIGN KEY (field_id, block_id, field_type) REFERENCES form_field (id, block_id, type),
    FOREIGN KEY (block_id, block_family) REFERENCES form_block_family (block_id, family),
    FOREIGN KEY (option_id, field_id) REFERENCES form_field_option (id, field_id),
    CHECK ((value_number IS NOT NULL) + (value_text IS NOT NULL) + (value_date IS NOT NULL)
           + (value_boolean IS NOT NULL) + (option_id IS NOT NULL)
           = CASE field_type WHEN 'multi_option' THEN 0 ELSE 1 END),
    CHECK (CASE field_type
               WHEN 'number'  THEN value_number IS NOT NULL
               WHEN 'text'    THEN value_text IS NOT NULL
               WHEN 'date'    THEN value_date IS NOT NULL
               WHEN 'boolean' THEN value_boolean IS NOT NULL
               WHEN 'option'  THEN option_id IS NOT NULL
               ELSE 1
           END)
);
CREATE TABLE productionorder_field_value_option (
    header_id  INTEGER NOT NULL,
    field_id   INTEGER NOT NULL,
    field_type TEXT    NOT NULL DEFAULT 'multi_option' CHECK (field_type = 'multi_option'),
    option_id  INTEGER NOT NULL,
    PRIMARY KEY (header_id, field_id, option_id),
    FOREIGN KEY (header_id, field_id, field_type)
        REFERENCES productionorder_field_value (header_id, field_id, field_type) ON DELETE CASCADE,
    FOREIGN KEY (option_id, field_id) REFERENCES form_field_option (id, field_id)
);
-- Dbmate schema migrations
INSERT INTO "schema_migrations" (version) VALUES
  ('20261103133615');
