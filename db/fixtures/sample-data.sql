PRAGMA foreign_keys = ON;

BEGIN;

INSERT INTO party_issuer (name, ruc, address, district) VALUES
    ('MALL SERVICE SAC', '20113126087', 'Av La Marina N° 2000 urb Pando - San Miguel', 'San Miguel')
ON CONFLICT DO NOTHING;

INSERT INTO party_client (name, ruc, address, district) VALUES
    ('Bebidas Andinas SAC',        '20999990001', 'Av. Javier Prado Este 4200',  'Santiago de Surco'),
    ('Telefonía Pacífico SAC',     '20999990002', 'Av. República de Panamá 3055', 'San Isidro'),
    ('Moda Urbana Perú SAC',       '20999990003', 'Jr. de la Unión 820',          'Cercado de Lima'),
    ('Juguetería Arcoíris EIRL',   '20999990004', 'Av. La Marina 2355',           'San Miguel'),
    ('Medios Creativos SAC',       '20999990005', 'Calle Las Begonias 441',       'San Isidro'),
    ('Central de Medios Sur SAC',  '20999990006', 'Av. Benavides 1580',           'Miraflores')
ON CONFLICT DO NOTHING;

WITH v(ruc, name, role, phone, email) AS (VALUES
    ('20999990001', 'Carla Mendoza',   'Gerente de Marca',        '999100101', 'carla.mendoza@example.com'),
    ('20999990001', 'Jorge Salinas',   'Analista de Pagos',       '999100102', 'pagos@example.com'),
    ('20999990002', 'Rosa Quispe',     'Jefa de Trade Marketing', '999100201', 'rosa.quispe@example.com'),
    ('20999990002', 'Diego Paredes',   'Tesorería',               '999100202', 'tesoreria@example.com'),
    ('20999990003', 'Lucía Fernández', 'Coordinadora Visual',     '999100301', 'lucia.fernandez@example.com'),
    ('20999990004', 'Martín Huamán',   'Dueño',                   '999100401', 'martin.huaman@example.com'),
    ('20999990005', 'Sofía Rojas',     'Ejecutiva de Cuentas',    '999100501', 'sofia.rojas@example.com'),
    ('20999990006', 'Andrés Vela',     'Planner de Medios',       '999100601', 'andres.vela@example.com')
)
INSERT INTO party_contact (client_id, name, role, phone, email)
SELECT c.id, v.name, v.role, v.phone, v.email
FROM v
JOIN party_client c ON c.ruc = v.ruc
WHERE NOT EXISTS (SELECT 1 FROM party_contact x WHERE x.client_id = c.id AND x.email = v.email);

WITH v(name, role, phone, email) AS (VALUES
    ('Valeria Campos', 'Jefa de Publicidad',     '999200001', 'publicidad@example.com'),
    ('Renzo Ortiz',    'Ejecutivo Comercial',    '999200002', 'comercial@example.com')
)
INSERT INTO party_contact (issuer_id, name, role, phone, email)
SELECT i.id, v.name, v.role, v.phone, v.email
FROM v
JOIN party_issuer i ON i.ruc = '20113126087'
WHERE NOT EXISTS (SELECT 1 FROM party_contact x WHERE x.issuer_id = i.id AND x.email = v.email);

INSERT INTO form_block (code, title, active) VALUES
    ('campaign',         'Datos de campaña',            1),
    ('art_specs',        'Especificaciones de arte',    1),
    ('activation',       'Activación / evento',         1),
    ('technical',        'Producción técnica',          1),
    ('billing_extra',    'Requisitos de facturación',   1),
    ('survey',           'Encuesta de satisfacción',    0)
ON CONFLICT DO NOTHING;

WITH v(code, family, rent_kind, production_kind, position) AS (VALUES
    ('campaign',      'rent',       NULL,         NULL, 1),
    ('campaign',      'production', NULL,         NULL, 1),
    ('art_specs',     'rent',       'ad_space',   NULL, 2),
    ('art_specs',     'production', NULL,         NULL, 2),
    ('activation',    'rent',       'temp_space', NULL, 3),
    ('technical',     'production', NULL,         'production', 3),
    ('billing_extra', 'rent',       NULL,         NULL, 9),
    ('billing_extra', 'production', NULL,         NULL, 9),
    ('survey',        'rent',       NULL,         NULL, 10)
)
INSERT INTO form_block_family (block_id, family, rent_contract_kind_id, production_contract_kind_id, position)
SELECT b.id, v.family,
       (SELECT id FROM rentorder_contract_kind WHERE code = v.rent_kind),
       (SELECT id FROM productionorder_contract_kind WHERE code = v.production_kind),
       v.position
FROM v
JOIN form_block b ON b.code = v.code
WHERE true
ON CONFLICT DO NOTHING;

WITH v(block, code, label, type, required, position, active) AS (VALUES
    ('campaign',      'name',          'Nombre de campaña',             'text',         1, 1, 1),
    ('campaign',      'audience',      'Público objetivo',              'multi_option', 0, 2, 1),
    ('campaign',      'budget',        'Presupuesto referencial (S/)',  'number',       0, 3, 1),
    ('campaign',      'launch',        'Fecha de lanzamiento',          'date',         0, 4, 1),
    ('art_specs',     'format',        'Formato',                       'option',       1, 1, 1),
    ('art_specs',     'dpi',           'Resolución (dpi)',              'number',       0, 2, 1),
    ('art_specs',     'art_due',       'Entrega de arte',               'date',         1, 3, 1),
    ('art_specs',     'finishes',      'Acabados',                      'multi_option', 0, 4, 1),
    ('art_specs',     'install',       'Requiere instalación',          'boolean',      0, 5, 1),
    ('art_specs',     'notes',         'Notas para diseño',             'text',         0, 6, 1),
    ('art_specs',     'legacy_code',   'Código antiguo',                'text',         0, 7, 0),
    ('activation',    'kind',          'Tipo de activación',            'option',       1, 1, 1),
    ('activation',    'promoters',     'Número de promotores',          'number',       1, 2, 1),
    ('activation',    'setup',         'Fecha de montaje',              'date',         0, 3, 1),
    ('activation',    'security',      'Requiere seguridad',            'boolean',      0, 4, 1),
    ('activation',    'services',      'Servicios requeridos',          'multi_option', 0, 5, 1),
    ('activation',    'stand',         'Descripción del stand',         'text',         0, 6, 1),
    ('technical',     'print',         'Tipo de impresión',             'option',       1, 1, 1),
    ('technical',     'substrate',     'Material de soporte',           'option',       0, 2, 1),
    ('technical',     'design',        'Incluye diseño',                'boolean',      0, 3, 1),
    ('technical',     'lead_days',     'Plazo de entrega (días)',       'number',       1, 4, 1),
    ('technical',     'notes',         'Observaciones técnicas',        'text',         0, 5, 1),
    ('billing_extra', 'po',            'Orden de compra del cliente',   'text',         0, 1, 1),
    ('billing_extra', 'statement',     'Requiere estado de cuenta',     'boolean',      0, 2, 1),
    ('billing_extra', 'formats',       'Formatos de factura',           'multi_option', 0, 3, 1),
    ('survey',        'score',         'Calificación',                  'number',       0, 1, 1)
)
INSERT INTO form_field (block_id, code, label, type, required, position, active)
SELECT b.id, v.code, v.label, v.type, v.required, v.position, v.active
FROM v
JOIN form_block b ON b.code = v.block
WHERE true
ON CONFLICT DO NOTHING;

WITH v(block, field, code, label, position, active) AS (VALUES
    ('campaign',      'audience',  'families',   'Familias',            1, 1),
    ('campaign',      'audience',  'youth',      'Jóvenes',             2, 1),
    ('campaign',      'audience',  'executives', 'Ejecutivos',          3, 1),
    ('campaign',      'audience',  'kids',       'Niños',               4, 1),
    ('art_specs',     'format',    'vinyl',      'Vinil',               1, 1),
    ('art_specs',     'format',    'banner',     'Lona',                2, 1),
    ('art_specs',     'format',    'backlight',  'Backlight',           3, 1),
    ('art_specs',     'format',    'digital',    'Pantalla digital',    4, 1),
    ('art_specs',     'format',    'paper',      'Papel (descontinuado)', 5, 0),
    ('art_specs',     'finishes',  'matte',      'Laminado mate',       1, 1),
    ('art_specs',     'finishes',  'gloss',      'Laminado brillante',  2, 1),
    ('art_specs',     'finishes',  'diecut',     'Troquelado',          3, 1),
    ('art_specs',     'finishes',  'eyelets',    'Ojales',              4, 1),
    ('activation',    'kind',      'tasting',    'Degustación',         1, 1),
    ('activation',    'kind',      'exhibit',    'Exhibición',          2, 1),
    ('activation',    'kind',      'sales',      'Venta',               3, 1),
    ('activation',    'kind',      'sampling',   'Sampling',            4, 1),
    ('activation',    'services',  'power',      'Punto eléctrico',     1, 1),
    ('activation',    'services',  'water',      'Agua',                2, 1),
    ('activation',    'services',  'wifi',       'Wifi',                3, 1),
    ('activation',    'services',  'storage',    'Almacén',             4, 1),
    ('technical',     'print',     'large',      'Gran formato',        1, 1),
    ('technical',     'print',     'screen',     'Serigrafía',          2, 1),
    ('technical',     'print',     'laser',      'Corte láser',         3, 1),
    ('technical',     'print',     'digital',    'Impresión digital',   4, 1),
    ('technical',     'substrate', 'foam',       'Foam',                1, 1),
    ('technical',     'substrate', 'pvc',        'PVC',                 2, 1),
    ('technical',     'substrate', 'mdf',        'MDF',                 3, 1),
    ('technical',     'substrate', 'acrylic',    'Acrílico',            4, 1),
    ('billing_extra', 'formats',   'pdf',        'PDF',                 1, 1),
    ('billing_extra', 'formats',   'xml',        'XML',                 2, 1)
)
INSERT INTO form_field_option (field_id, field_type, code, label, position, active)
SELECT f.id, f.type, v.code, v.label, v.position, v.active
FROM v
JOIN form_block b ON b.code = v.block
JOIN form_field f ON f.block_id = b.id AND f.code = v.field
WHERE true
ON CONFLICT DO NOTHING;

INSERT INTO order_number (year, seq, family) VALUES
    (2026, 901, 'rent'),
    (2026, 902, 'rent'),
    (2026, 903, 'production')
ON CONFLICT DO NOTHING;

WITH v(seq, kind, issued_on, ruc, brand, contact_email, electric_power, guarantee, guarantee_amount, billing_ruc, billing_email, payment_terms, agency_ruc, observations) AS (VALUES
    (901, 'ad_space',   '2026-11-02', '20999990001', 'Inca Fresh', 'carla.mendoza@example.com', 1, 0, NULL,
     '20999990001', 'pagos@example.com', '30 días fecha de factura', '20999990005', 'Campaña navideña en pantallas del hall principal.'),
    (902, 'temp_space', '2026-11-03', '20999990002', 'Pacífico 5G', 'rosa.quispe@example.com', 1, 1, 450000,
     '20999990002', 'tesoreria@example.com', 'Contado', NULL, 'Activación de fin de semana con stand en la plaza central.')
)
INSERT INTO rentorder_header
    (order_number_id, order_contract_kind_id, issued_on, client_id, brand, contact_id, issuer_id, issuer_contact_id,
     electric_power, guarantee, guarantee_amount, billing_client_id, billing_contact_id, payment_terms, agency_client_id, observations)
SELECT n.id, k.id, unixepoch(v.issued_on), c.id, v.brand,
       (SELECT id FROM party_contact WHERE client_id = c.id AND email = v.contact_email),
       i.id,
       (SELECT id FROM party_contact WHERE issuer_id = i.id AND email = 'publicidad@example.com'),
       v.electric_power, v.guarantee, v.guarantee_amount, bc.id,
       (SELECT id FROM party_contact WHERE client_id = bc.id AND email = v.billing_email),
       v.payment_terms,
       (SELECT id FROM party_client WHERE ruc = v.agency_ruc),
       v.observations
FROM v
JOIN order_number n ON n.year = 2026 AND n.seq = v.seq AND n.family = 'rent'
JOIN rentorder_contract_kind k ON k.code = v.kind
JOIN party_client c ON c.ruc = v.ruc
JOIN party_client bc ON bc.ruc = v.billing_ruc
JOIN party_issuer i ON i.ruc = '20113126087'
WHERE true
ON CONFLICT DO NOTHING;

INSERT INTO productionorder_header
    (order_number_id, order_contract_kind_id, issued_on, client_id, brand, contact_id, issuer_id, issuer_contact_id,
     billing_client_id, billing_contact_id, payment_terms, observations)
SELECT n.id, k.id, unixepoch('2026-11-04'), c.id, 'Arcoíris Kids',
       (SELECT id FROM party_contact WHERE client_id = c.id AND email = 'martin.huaman@example.com'),
       i.id,
       (SELECT id FROM party_contact WHERE issuer_id = i.id AND email = 'comercial@example.com'),
       c.id,
       (SELECT id FROM party_contact WHERE client_id = c.id AND email = 'martin.huaman@example.com'),
       '50% adelanto, 50% contra entrega',
       'Producción de displays para vitrina navideña.'
FROM order_number n
JOIN productionorder_contract_kind k ON k.code = 'production'
JOIN party_client c ON c.ruc = '20999990004'
JOIN party_issuer i ON i.ruc = '20113126087'
WHERE n.year = 2026 AND n.seq = 903 AND n.family = 'production'
ON CONFLICT DO NOTHING;

WITH v(position, code, element, location, motive, start_date, end_date, qty, periods, unit, unit_price) AS (VALUES
    (1, 'PD-01', 'Pantalla digital', 'Hall principal, nivel 1', 'Inca Fresh Navidad', '2026-12-01', '2026-12-31', 2, 1, 'month', 350000),
    (2, 'BL-07', 'Backlight',        'Patio de comidas',        'Inca Fresh Navidad', '2026-12-01', '2026-12-31', 4, 1, 'month', 120000)
)
INSERT INTO rentorder_ad_space_line
    (header_id, position, code, element, location, motive, start_date, end_date, element_qty, periods, period_unit, unit_price, total)
SELECT h.id, v.position, v.code, v.element, v.location, v.motive, unixepoch(v.start_date), unixepoch(v.end_date),
       v.qty, v.periods, v.unit, v.unit_price, v.qty * v.periods * v.unit_price
FROM v
JOIN rentorder_header h ON h.order_number_id = (SELECT id FROM order_number WHERE year = 2026 AND seq = 901 AND family = 'rent')
WHERE true
ON CONFLICT DO NOTHING;

INSERT INTO rentorder_temp_space_line
    (header_id, position, code, location, area, motive, description, schedule, start_date, end_date, days, total)
SELECT h.id, 1, 'ET-03', 'Plaza central', '6x4 m', 'Pacífico 5G', 'Stand con demostración de equipos', '10:00–22:00',
       unixepoch('2026-11-21'), unixepoch('2026-11-22'), 2, 900000
FROM rentorder_header h
WHERE h.order_number_id = (SELECT id FROM order_number WHERE year = 2026 AND seq = 902 AND family = 'rent')
ON CONFLICT DO NOTHING;

WITH v(position, code, location, element, material, motive, quantity, width, height, total) AS (VALUES
    (1, 'PR-11', 'Vitrina tienda 104', 'Display de piso', 'Foam 10 mm',  'Arcoíris Navidad', 3, 0.8, 1.8, 165000),
    (2, 'PR-12', 'Vitrina tienda 104', 'Cenefa',          'Vinil + PVC', 'Arcoíris Navidad', 1, 3.0, 0.4,  72000)
)
INSERT INTO productionorder_line
    (header_id, position, code, location, element, material, motive, quantity, width, height, total)
SELECT h.id, v.position, v.code, v.location, v.element, v.material, v.motive, v.quantity, v.width, v.height, v.total
FROM v
JOIN productionorder_header h ON h.order_number_id = (SELECT id FROM order_number WHERE year = 2026 AND seq = 903 AND family = 'production')
WHERE true
ON CONFLICT DO NOTHING;

CREATE TEMP VIEW sample_field AS
SELECT b.code AS block, f.code AS field, f.id AS field_id, f.block_id, f.type
FROM form_field f JOIN form_block b ON b.id = f.block_id;

CREATE TEMP VIEW sample_option AS
SELECT b.code AS block, f.code AS field, o.code AS option, o.id AS option_id
FROM form_field_option o JOIN form_field f ON f.id = o.field_id JOIN form_block b ON b.id = f.block_id;

WITH v(seq, block, field, num, txt, dt, bool, opt) AS (VALUES
    (901, 'campaign',      'name',      NULL, 'Navidad Inca Fresh 2026', NULL, NULL, NULL),
    (901, 'campaign',      'audience',  NULL, NULL, NULL, NULL, NULL),
    (901, 'campaign',      'budget',    25000, NULL, NULL, NULL, NULL),
    (901, 'campaign',      'launch',    NULL, NULL, '2026-12-01', NULL, NULL),
    (901, 'art_specs',     'format',    NULL, NULL, NULL, NULL, 'digital'),
    (901, 'art_specs',     'dpi',       150, NULL, NULL, NULL, NULL),
    (901, 'art_specs',     'art_due',   NULL, NULL, '2026-11-25', NULL, NULL),
    (901, 'art_specs',     'finishes',  NULL, NULL, NULL, NULL, NULL),
    (901, 'art_specs',     'install',   NULL, NULL, NULL, 1, NULL),
    (901, 'billing_extra', 'po',        NULL, 'OC-2026-1187', NULL, NULL, NULL),
    (901, 'billing_extra', 'formats',   NULL, NULL, NULL, NULL, NULL),
    (902, 'campaign',      'name',      NULL, 'Lanzamiento Pacífico 5G', NULL, NULL, NULL),
    (902, 'activation',    'kind',      NULL, NULL, NULL, NULL, 'exhibit'),
    (902, 'activation',    'promoters', 4, NULL, NULL, NULL, NULL),
    (902, 'activation',    'setup',     NULL, NULL, '2026-11-20', NULL, NULL),
    (902, 'activation',    'security',  NULL, NULL, NULL, 1, NULL),
    (902, 'activation',    'services',  NULL, NULL, NULL, NULL, NULL),
    (902, 'activation',    'stand',     NULL, 'Mostrador curvo con dos pantallas y zona de prueba.', NULL, NULL, NULL),
    (902, 'billing_extra', 'statement', NULL, NULL, NULL, 0, NULL)
)
INSERT INTO rentorder_field_value (header_id, field_id, block_id, field_type, value_number, value_text, value_date, value_boolean, option_id)
SELECT h.id, sf.field_id, sf.block_id, sf.type, v.num, v.txt, unixepoch(v.dt), v.bool,
       (SELECT option_id FROM sample_option so WHERE so.block = v.block AND so.field = v.field AND so.option = v.opt)
FROM v
JOIN sample_field sf ON sf.block = v.block AND sf.field = v.field
JOIN rentorder_header h ON h.order_number_id = (SELECT id FROM order_number WHERE year = 2026 AND seq = v.seq AND family = 'rent')
WHERE true
ON CONFLICT DO NOTHING;

WITH v(seq, block, field, opt) AS (VALUES
    (901, 'campaign',      'audience', 'families'),
    (901, 'campaign',      'audience', 'kids'),
    (901, 'art_specs',     'finishes', 'gloss'),
    (901, 'billing_extra', 'formats',  'pdf'),
    (901, 'billing_extra', 'formats',  'xml'),
    (902, 'activation',    'services', 'power'),
    (902, 'activation',    'services', 'wifi')
)
INSERT INTO rentorder_field_value_option (header_id, field_id, option_id)
SELECT h.id, so_field.field_id, so.option_id
FROM v
JOIN sample_field so_field ON so_field.block = v.block AND so_field.field = v.field
JOIN sample_option so ON so.block = v.block AND so.field = v.field AND so.option = v.opt
JOIN rentorder_header h ON h.order_number_id = (SELECT id FROM order_number WHERE year = 2026 AND seq = v.seq AND family = 'rent')
WHERE true
ON CONFLICT DO NOTHING;

WITH v(block, field, num, txt, dt, bool, opt) AS (VALUES
    ('campaign',  'name',      NULL, 'Vitrina Navidad Arcoíris', NULL, NULL, NULL),
    ('art_specs', 'format',    NULL, NULL, NULL, NULL, 'vinyl'),
    ('art_specs', 'art_due',   NULL, NULL, '2026-11-15', NULL, NULL),
    ('technical', 'print',     NULL, NULL, NULL, NULL, 'large'),
    ('technical', 'substrate', NULL, NULL, NULL, NULL, 'foam'),
    ('technical', 'design',    NULL, NULL, NULL, 1, NULL),
    ('technical', 'lead_days', 7, NULL, NULL, NULL, NULL),
    ('technical', 'notes',     NULL, 'Bordes redondeados; entregar embalado por pieza.', NULL, NULL, NULL)
)
INSERT INTO productionorder_field_value (header_id, field_id, block_id, field_type, value_number, value_text, value_date, value_boolean, option_id)
SELECT h.id, sf.field_id, sf.block_id, sf.type, v.num, v.txt, unixepoch(v.dt), v.bool,
       (SELECT option_id FROM sample_option so WHERE so.block = v.block AND so.field = v.field AND so.option = v.opt)
FROM v
JOIN sample_field sf ON sf.block = v.block AND sf.field = v.field
JOIN productionorder_header h ON h.order_number_id = (SELECT id FROM order_number WHERE year = 2026 AND seq = 903 AND family = 'production')
WHERE true
ON CONFLICT DO NOTHING;

DROP VIEW sample_field;
DROP VIEW sample_option;

COMMIT;
