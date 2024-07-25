DROP TABLE IF EXISTS ${SCHEMA}.teams;

CREATE TABLE IF NOT EXISTS ${SCHEMA}.teams (
    id TEXT PRIMARY KEY NOT NULL,
    name TEXT NOT NULL
);

DROP TABLE IF EXISTS ${SCHEMA}.employee_team;

CREATE TABLE IF NOT EXISTS ${SCHEMA}.employee_team (
  employee_id TEXT NOT NULL,
  team_id TEXT NOT NULL,
  PRIMARY KEY (employee_id, team_id),
  FOREIGN KEY (employee_id) REFERENCES ${SCHEMA}.employees (id) ON DELETE CASCADE,
  FOREIGN KEY (team_id) REFERENCES ${SCHEMA}.teams (id) ON DELETE CASCADE
);
